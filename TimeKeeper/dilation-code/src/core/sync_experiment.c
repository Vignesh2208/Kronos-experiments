#include "dilation_module.h"

/*
Contains most of the functions in dealing with keeping an experiment synchronized within CORE.
Basic Flow of Functions for a synchronized experiment:
	-For every container, add_to_exp_proc is called, which calls add_to_exp, when passed the PID of the container,
		initializes the container and assigns it to a CPU, sched_priority and so forth
	-When all containers have been 'added', core_sync_exp is called, which will start the experiment.
	-Each container will run for its specified time, when its time is up, the timer is triggered, and exp_hrtimer_callback is called, which will call freeze_proc_exp_recurse to freeze the container, and unfreeze_proc_exp_recurse to unfreeze the next container
	-When the last containers timer is triggered, the catchup_func is woken up, and the synchronization phase begins.
	-At the end of the catchup_func, it will restart the round, and call unfreeze_proc_exp_recurse on the appropriate containers
	-When you want to end the experiment, set_clean_exp is called, telling catchup_func to stop at the end of the current round
	-When the end of the round is reached, catchup_func will call clean_exp and stop the experiment.
*/

void calcExpectedIncrease(void);
void calcTaskRuntime(struct dilation_task_struct * task);
struct dilation_task_struct * getNextRunnableTask(struct dilation_task_struct * task);
int catchup_func(void *data);
int calculate_sync_drift(void *data);
enum hrtimer_restart exp_hrtimer_callback( struct hrtimer *timer);
enum hrtimer_restart alt_hrtimer_callback( struct hrtimer * timer );


/* Local Functions */
void add_to_exp(int pid);
void addToChain(struct dilation_task_struct *task);
void assign_to_cpu(struct dilation_task_struct *task);
void printChainInfo(void);
void add_to_exp_proc(char *write_buffer);
void clean_exp(void);
void set_clean_exp(void);
void set_cbe_exp_timeslice(char *write_buffer);
void set_children_time(struct task_struct *aTask, s64 time);
int freeze_children(struct task_struct *aTask, s64 time);
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time,struct dilation_task_struct *lxc);
int resume_all(struct task_struct *aTask,struct dilation_task_struct * lxc) ;
int freeze_proc_exp_recurse(struct dilation_task_struct *aTask);
int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time);
void core_sync_exp(void);
void set_children_policy(struct task_struct *aTask, int policy, int priority);
void set_children_cpu(struct task_struct *aTask, int cpu);
void add_sim_to_exp_proc(char *write_buffer);
void clean_stopped_containers(void);
void dilate_proc_recurse_exp(int pid, int new_dilation);
void change_containers_dilation(void);
void sync_and_freeze(void);
void calculate_virtual_time_difference(struct dilation_task_struct* task, s64 now, s64 expected_time);
s64 calculate_change(struct dilation_task_struct* task, s64 virt_time, s64 expected_time);
s64 get_virtual_time(struct dilation_task_struct* task, s64 now);
int run_head_process(struct dilation_task_struct * lxc, lxc_schedule_elem * head, s64 start_time, s64 vt_advance);
int unfreeze_proc_vt_advance(struct dilation_task_struct *aTask, s64 expected_time) ;
void set_all_freeze_times_recurse(struct task_struct * aTask, s64 freeze_time,s64 last_ppp, int max_no_recursions);
void set_all_past_physical_times_recurse(struct task_struct * aTask, s64 time, int max_no_of_recursions, struct dilation_task_struct * lxc);
extern void unfreeze_all(struct task_struct *aTask);


void print_schedule_list(struct dilation_task_struct * lxc)
{
	int i = 0;
	lxc_schedule_elem * curr;
	if(lxc != NULL) {
		for(i = 0; i < schedule_list_size(lxc); i++){
			curr = llist_get(&lxc->schedule_queue, i);
			if(curr != NULL) {
				PDEBUG_V("Schedule List Item No: %d, Item: %d, LXC: %d, Size: %d\n",i, curr->pid, lxc->linux_task->pid,schedule_list_size(lxc));
			}
		}	
	}
}

/* Local Variables */

s64 PRECISION = 1000;  
s64 FREEZE_QUANTUM = 300000000;
s64 Sim_time_scale = 1;
s64 boottime;
atomic_t is_boottime_set = ATOMIC_INIT(0);
hashmap poll_process_lookup;
hashmap select_process_lookup;
hashmap sleep_process_lookup;

/* the number of containers in the experiment */
int proc_num = 0;                           

/* the task that calls the synchronization function (catchup_func) */
struct task_struct *catchup_task;           

/* the leader task, aka the task with the highest dilation factor */
struct dilation_task_struct *leader_task;   

/* represents the highest dilation factor in the experiment (aka the TDF of the leader) */
int exp_highest_dilation = -100000000;      

/* the linked list of all containers in the experiment */
struct list_head exp_list; 

/* the mutex for exp_list to ensure we do not have race conditions */
struct mutex exp_mutex; 

/* if == -1 then the experiment is not started yet, if == 0 then the experiment is currently running, if == 1 then the experiment is set to be stopped at the end of the current round. */
int experiment_stopped; 

/* every CPU has a linked list of containers assinged to that CPU. This variable specifies the 'head' container for each CPU */
struct dilation_task_struct* chainhead[EXP_CPUS]; 

/* for every cpu, this value represents how long every container for that CPU will need to run in each round (TimeKeeper will assign a new container to the CPU with the lowest value) */
s64 chainlength[EXP_CPUS]; 

struct task_struct* chaintask[EXP_CPUS];
int values[EXP_CPUS];

/* The virtual time that every container should be at (or at least close to) at the end of every round */
s64 actual_time; 

/* specifies how many head containers are in the experiment. This number will most often be equal to EXP_CPUS. Handles the special case if containers < EXP_CPUS so we do not have an array index out of bounds error */
int number_of_heads = 0; 

/* How much virtual time should increase at each round */
s64 expected_increase; 

/* flag if dilation of a container has changed in the experiment. 0 means no changes, 1 means change */
int dilation_change = 0; 

/* CBE for ns-3/core, CS for S3F (CS) */
int experiment_type = NOTSET; 
int stopped_change = 0;

extern s64 round_error;
extern s64 n_rounds;
extern s64 round_error_sq;


/* synchronization variables to support parallelization */

static DECLARE_WAIT_QUEUE_HEAD(wq);
atomic_t n_active_syscalls = ATOMIC_INIT(0);
atomic_t experiment_stopping = ATOMIC_INIT(0);
atomic_t worker_count = ATOMIC_INIT(0);
atomic_t running_done = ATOMIC_INIT(0);
atomic_t start_count = ATOMIC_INIT(0);
atomic_t catchup_Task_finished = ATOMIC_INIT(0); 
atomic_t woke_up_catchup_Task = ATOMIC_INIT(0);	
atomic_t wake_up_signal[EXP_CPUS];
atomic_t wake_up_signal_sync_drift[EXP_CPUS];
atomic_t progress_cbe_rounds = ATOMIC_INIT(0);
atomic_t progress_cbe_enabled = ATOMIC_INIT(0);

int curr_process_finished_flag[EXP_CPUS];
static int curr_sync_task_finished_flag[EXP_CPUS];
static wait_queue_head_t per_cpu_wait_queue[EXP_CPUS];
static wait_queue_head_t per_cpu_sync_task_queue[EXP_CPUS];
static wait_queue_head_t progress_cbe_wait_queue;
static wait_queue_head_t progress_cbe_catchup_tsk;
static wait_queue_head_t cbe_exp_stop_queue;

// externs
extern struct poll_list {
    struct poll_list *next;
    int len;
    struct pollfd entries[0];
};
extern int do_dialated_poll(unsigned int nfds,  struct poll_list *list, struct poll_wqueues *wait,struct task_struct * tsk);
extern int do_dialated_select(int n, fd_set_bits *fds,struct task_struct * tsk);
extern struct task_struct *loop_task;
extern int TOTAL_CPUS;
extern struct timeline* timelineHead[EXP_CPUS];
extern void perform_on_children(struct task_struct *aTask, void(*action)(int,int), int val);
extern void change_dilation(int pid, int new_dilation);
extern s64 get_virtual_time_task(struct task_struct* task, s64 now);

// system call orig function pointers
extern asmlinkage int (*ref_sys_poll)(struct pollfd __user * ufds, unsigned int nfds, int timeout_msecs);
extern asmlinkage long (*ref_sys_select)(int n, fd_set __user *inp, fd_set __user *outp, fd_set __user *exp, struct timeval __user *tvp);
extern asmlinkage long (*ref_sys_sleep)(struct timespec __user *rqtp, struct timespec __user *rmtp);
extern asmlinkage long (*ref_sys_clock_nanosleep)(const clockid_t which_clock, int flags, const struct timespec __user * rqtp, struct timespec __user * rmtp);
extern asmlinkage int (*ref_sys_clock_gettime)(const clockid_t which_clock, struct timespec __user * tp);


extern unsigned long **sys_call_table; //address of the sys_call_table, so we can hijack certain system calls
unsigned long orig_cr0;


int progress_exp_cbe(char * write_buffer){

	int progress_rounds = 0;
	int ret = 0;
	if(experiment_type != CBE)
		return -1;
	
	set_current_state(TASK_INTERRUPTIBLE);	
	progress_rounds = atoi(write_buffer);
	if(progress_rounds > 0 )
		atomic_set(&progress_cbe_rounds,progress_rounds);
	else
		atomic_set(&progress_cbe_rounds,1);	
		
	atomic_set(&progress_cbe_enabled,1);
	PDEBUG_V("Progress CBE - initiated. Number of Progress rounds = %d\n", progress_rounds);
	wake_up_interruptible(&progress_cbe_catchup_tsk);
	do{
			ret = wait_event_interruptible_timeout(progress_cbe_wait_queue,atomic_read(&progress_cbe_rounds) == 0,HZ);
			if(ret == 0)
				set_current_state(TASK_INTERRUPTIBLE);
			else
				set_current_state(TASK_RUNNING);

	}while(ret == 0);
	
	return 0;
}

void resume_exp_cbe(){

	if(atomic_read(&progress_cbe_enabled) == 1 && atomic_read(&progress_cbe_rounds) <= 0 && experiment_type == CBE) {
		atomic_set(&progress_cbe_enabled,0);
		atomic_set(&progress_cbe_rounds,0);
		wake_up_interruptible(&progress_cbe_catchup_tsk);
	
	}

}


/***
Given a pid and a new dilation, dilate it and all of it's children
***/
void dilate_proc_recurse_exp(int pid, int new_dilation) {
	struct task_struct *aTask;
    aTask = find_task_by_pid(pid);
    if (aTask != NULL) {
		change_dilation(pid, new_dilation);
		perform_on_children(aTask, change_dilation, new_dilation);
	}
}

/*** 
Gets the virtual time given a dilation_task_struct
***/
s64 get_virtual_time(struct dilation_task_struct* task, s64 now) {
	s64 virt_time;
	virt_time = get_virtual_time_task(task->linux_task, now);
    task->curr_virt_time = virt_time;
	return virt_time;
}


/*
Changes the FREEZE_QUANTUM for a CBE experiment
*/
void set_cbe_exp_timeslice(char *write_buffer){

	s64 timeslice;
	timeslice = atoi(write_buffer);
	if(experiment_type == CBE){
		FREEZE_QUANTUM = timeslice;
		FREEZE_QUANTUM = FREEZE_QUANTUM*Sim_time_scale;
		PDEBUG_A("Set CBE Exp Timeslice: Set Freeze Quantum : %lld\n", FREEZE_QUANTUM);
	}

}


/***
Adds the simulator pid to the experiment - might be deprecated.
***/
void add_sim_to_exp_proc(char *write_buffer) {
	int pid;
    pid = atoi(write_buffer);
	add_to_exp(pid);
}

/*
Creates and initializes dilation_task_struct given a task_struct.
*/
struct dilation_task_struct* initialize_node(struct task_struct* aTask) {
	struct dilation_task_struct* list_node;
	PDEBUG_A("Initialize Node: Adding a pid: %d Num of Nodes in experiment so Far: %d, Number of CPUS used so Far: %d\n", aTask->pid, proc_num, number_of_heads);
	list_node = (struct dilation_task_struct *)kmalloc(sizeof(struct dilation_task_struct), GFP_KERNEL);
	list_node->linux_task = aTask;
	list_node->stopped = 0;
	list_node->next = NULL;
	list_node->prev = NULL;
	list_node->wake_up_time = 0;
	list_node->newDilation = -1;
	list_node->increment = 0;
	list_node->cpu_assignment = -1;
	list_node->rr_run_time = 0;
	list_node->last_timer_fire_time = 0;
	list_node->last_timer_duration = 0;
	
	list_node->last_run = NULL;
	llist_init(&list_node->schedule_queue);
	hmap_init(&list_node->valid_children,"int",0);
	hrtimer_init( &list_node->timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS );
	hrtimer_init( &list_node->schedule_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	return list_node;
}

/***
wake_up_process returns 1 if it was stopped and gets woken up, 0 if it is dead OR already running. Used with CS experiments.
***/
void progress_exp(void) {
        wake_up_process(catchup_task);
}

/***
Reads a PID from the buffer, and adds the corresponding task to the experiment (i believe this does not support the adding
of processes if the experiment has alreaded started)
***/
void add_to_exp_proc(char *write_buffer) {
    int pid;
    pid = atoi(write_buffer);

	if (experiment_type == CS) {
		PDEBUG_A("Add To Exp Proc: Trying to add to wrong experiment type.. exiting\n");
	}
	else if (experiment_stopped == NOTRUNNING) {
        add_to_exp(pid);
	}
	else {
		PDEBUG_A("Add to Exp Proc: Trying to add a LXC to experiment that is already running\n");
	}
}

/***
Gets called by add_to_exp_proc(). Initiazes a containers timer, sets scheduling policy.
***/
void add_to_exp(int pid) {
        struct task_struct* aTask;
        struct dilation_task_struct* list_node;
        aTask = find_task_by_pid(pid);
        experiment_stopped = NOTRUNNING;

        /* maybe I should just skip this pid instead of completely dropping out? */
        if (aTask == NULL)
        {
                PDEBUG_A("Add to Exp: Pid %d is invalid, Dropping out\n",pid);
                return;
        }

        proc_num++;
		experiment_type = CBE;
        if (EXP_CPUS < proc_num)
                number_of_heads = EXP_CPUS;
        else
                number_of_heads = proc_num;

		list_node = initialize_node(aTask);
        list_node->timer.function = &exp_hrtimer_callback;

        mutex_lock(&exp_mutex);
        list_add(&(list_node->list), &exp_list);
        mutex_unlock(&exp_mutex);
        if (exp_highest_dilation < list_node->linux_task->dilation_factor)
        {
                exp_highest_dilation = list_node->linux_task->dilation_factor;
                leader_task = list_node;
        }
}

/*
Sets all nodes added to the experiment to the same point in time, and freezes them
*/
void sync_and_freeze() {
	struct timeval now_timeval;
	s64 now;
	struct dilation_task_struct* list_node;
	struct list_head *pos;
	struct list_head *n;
	int i;
	int j;
	struct sched_param sp;
	int placed_lxcs;
	placed_lxcs = 0;
	unsigned long flags;

	PDEBUG_A("Sync And Freeze: ** Starting Experiment Synchronization **\n");

	if (proc_num == 0) {
		PDEBUG_A("Sync And Freeze: Nothing added to experiment, dropping out\n");
		return;
	}

	if (experiment_stopped != NOTRUNNING) {
        PDEBUG_A("Sync And Freeze: Trying to StartExp when an experiment is already running!\n");
        return;
    }


	hmap_init( &poll_process_lookup,"int",0);
	hmap_init( &select_process_lookup,"int",0);
	hmap_init( &sleep_process_lookup,"int",0);

	round_error = 0;
	round_error_sq = 0;
	n_rounds = 0;

	PDEBUG_V("Sync and Freeze: Hooking system calls\n");
	PDEBUG_V("Catchup Task: Pid = %d\n", catchup_task->pid);

	orig_cr0 = read_cr0();
	write_cr0(orig_cr0 & ~0x00010000);

	sys_call_table[NR_select] = (unsigned long *)sys_select_new;	
	sys_call_table[__NR_poll] = (unsigned long *) sys_poll_new;
	sys_call_table[__NR_nanosleep] = (unsigned long *)sys_sleep_new;
	sys_call_table[__NR_clock_gettime] = (unsigned long *) sys_clock_gettime_new;
	sys_call_table[__NR_clock_nanosleep] = (unsigned long *) sys_clock_nanosleep_new;

	write_cr0(orig_cr0 | 0x00010000 );


	for (j = 0; j < number_of_heads; j++) {
        values[j] = j;
	}
    
	sp.sched_priority = 99;
	
	if(experiment_type == CBE) {
		init_waitqueue_head(&progress_cbe_wait_queue);
		init_waitqueue_head(&progress_cbe_catchup_tsk);	
		init_waitqueue_head(&cbe_exp_stop_queue);
	}

	/* Create the threads for parallel computing */
	for (i = 0; i < number_of_heads; i++)
	{
		PDEBUG_A("Sync And Freeze: Adding Worker Thread %d\n", i);
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_type == CBE){
			init_waitqueue_head(&per_cpu_sync_task_queue[i]);
			curr_sync_task_finished_flag[i] = 0;
			//chaintask[i] = kthread_run(&calculate_sync_drift, &values[i], "worker");
			chaintask[i] = kthread_create(&calculate_sync_drift, &values[i], "worker");
			if(!IS_ERR(chaintask[i])) {
	            kthread_bind(chaintask[i],i % (TOTAL_CPUS - EXP_CPUS));
	            wake_up_process(chaintask[i]);
	            PDEBUG_A("Chain Task %d: Pid = %d\n", i, chaintask[i]->pid);
	        }


		}
	}

	/* If in CBE mode, find the leader task (highest TDF) */
	if (experiment_type == CBE) {
		list_for_each_safe(pos, n, &exp_list)
        	{
        		list_node = list_entry(pos, struct dilation_task_struct, list);
				if (list_node->linux_task->dilation_factor > exp_highest_dilation) {
                       		leader_task = list_node;
                        	exp_highest_dilation = list_node->linux_task->dilation_factor;
               	}
		}
	}

	/* calculate how far virtual time should advance every round */
    calcExpectedIncrease(); 

    do_gettimeofday(&now_timeval);
    now = timeval_to_ns(&now_timeval);
    actual_time = now;
    PDEBUG_A("Sync And Freeze: Setting the virtual start time of all tasks to be: %lld\n", actual_time);

    /* for every container in the experiment, set the virtual_start_time (so it starts at the same time), calculate
    how long each task should be allowed to run in each round, and freeze the container */
    list_for_each_safe(pos, n, &exp_list)
    {
        list_node = list_entry(pos, struct dilation_task_struct, list);
        if (experiment_type == CBE)
			calcTaskRuntime(list_node);

		/* consistent time */
        list_node->linux_task->virt_start_time = now; 
		if (experiment_type == CS) {
			list_node->expected_time = now;
			list_node->running_time = 0;
		}

	
		acquire_irq_lock(&list_node->linux_task->dialation_lock,flags);
		list_node->linux_task->past_physical_time = 0;
		list_node->linux_task->past_virtual_time = 0;
		list_node->linux_task->wakeup_time = 0;
		list_node->linux_task->freeze_time = now;
		list_node->linux_task->virt_start_time = now;
		release_irq_lock(&list_node->linux_task->dialation_lock,flags);
	
		/* freeze all children */
		freeze_proc_exp_recurse(list_node); 

		/* set priority and scheduling policy */
        if (list_node->stopped == -1) {
            PDEBUG_A("Sync And Freeze: One of the LXCs no longer exist.. exiting experiment\n");
            clean_exp();
            return;
        }

       	if (sched_setscheduler(list_node->linux_task, SCHED_RR, &sp) == -1 )
           	PDEBUG_A("Sync And Freeze: Error setting SCHED_RR %d\n",list_node->linux_task->pid);
        set_children_time(list_node->linux_task, now);
		set_children_policy(list_node->linux_task, SCHED_RR, sp.sched_priority);

		if (experiment_type == CS) {
				PDEBUG_A("Sync And Freeze: Cpus allowed! : %d \n", list_node->linux_task->cpus_allowed);
     			bitmap_zero((&list_node->linux_task->cpus_allowed)->bits, 8);
	        	cpumask_set_cpu(list_node->cpu_assignment, &list_node->linux_task->cpus_allowed);
				set_children_cpu(list_node->linux_task, list_node->cpu_assignment);
		}

		PDEBUG_A("Sync And Freeze: Task running time: %lld\n", list_node->running_time);
	}

	/* If in CBE mode, assign all tasks to a specfic CPU (this has already been done if in CS mode) */
	if (experiment_type == CBE) {
		while (placed_lxcs < proc_num) {
			int highest_tdf;
			struct dilation_task_struct* task_to_assign;
			highest_tdf = -100000000;
			task_to_assign = NULL;
			list_for_each_safe(pos, n, &exp_list)
			{
				list_node = list_entry(pos, struct dilation_task_struct, list);
				if (list_node->cpu_assignment == -1 && list_node->linux_task->dilation_factor > highest_tdf) {
					task_to_assign = list_node;
					highest_tdf = list_node->linux_task->dilation_factor;
				}
			}
			if (task_to_assign->linux_task->dilation_factor > exp_highest_dilation) {
				leader_task = task_to_assign;
				exp_highest_dilation = task_to_assign->linux_task->dilation_factor;
			}
			assign_to_cpu(task_to_assign);
			placed_lxcs++;
		}
	}
    printChainInfo();

	/* Set what mode experiment is in, depending on experiment_type (CBE or CS) */
	if (experiment_type == CS)
		experiment_stopped = RUNNING;
	else
		experiment_stopped = FROZEN;

//if its 64-bit, start the busy loop task to fix the weird bug
#ifdef __x86_64
	kill(loop_task, SIGCONT, NULL);
#endif
	PDEBUG_A("Finished Sync and Freeze\n");

}

/***
Specifies the start of the experiment (if in CBE mode)
***/
void core_sync_exp() {
	struct dilation_task_struct* list_node;
	int i;
	ktime_t ktime;

	if (experiment_type == CS) {
		PDEBUG_A("Core Sync Exp: Trying to start wrong type of experiment.. exiting\n");
		return;
	}
	if (experiment_stopped != FROZEN) {
		PDEBUG_A("Core Sync Exp: Experiment is not ready to commence, must run synchronizeAndFreeze\n");
		return;
	}

	/* for every 'head' container, unfreeze it and set its timer to fire at some point in the future (based off running_time) */
	PDEBUG_A("Core Sync Exp: Freezing all nodes\n");
	for (i=0; i<number_of_heads; i++)
	{
	    list_node = chainhead[i];
	 	freeze_proc_exp_recurse(list_node);

	}

	PDEBUG_A("Core Sync Exp: Waking up catchup task\n");
	experiment_stopped = RUNNING;
	while(wake_up_process(catchup_task) != 1);
	
	PDEBUG_A("Core Sync Exp: Woke up catchup task\n");
}

/***
If we know the process with the highest TDF in the experiment, we can calculate how far it should progress in virtual time,
and set the global variable 'expected_increase' accordingly.
***/
void calcExpectedIncrease() {
        s32 rem;
        if (exp_highest_dilation > 0)
        {
                expected_increase = div_s64_rem(FREEZE_QUANTUM*PRECISION,exp_highest_dilation,&rem);
                expected_increase += rem;
        }
        else if (exp_highest_dilation == 0)
                expected_increase = FREEZE_QUANTUM;
        else {
                expected_increase = div_s64_rem(FREEZE_QUANTUM*(exp_highest_dilation*-1), PRECISION, &rem);
        }
        return;
}

/***
Given a task with a TDF, determine how long it should be allowed to run in each round, stored in running_time field
***/
void calcTaskRuntime(struct dilation_task_struct * task) {
        int dil;
        s64 temp_proc;
        s64 temp_high;
        s32 rem;

        /* temp_proc and temp_high are temporary dilations for the container and leader respectively.
        this is done just to make sure the Math works (no divide by 0 errors if the TDF is 0, by making the temp TDF 1) */
        temp_proc = 0;
        temp_high = 0;
        if (exp_highest_dilation == 0)
                temp_high = 1;
        else if (exp_highest_dilation < 0)
                temp_high = exp_highest_dilation*-1;

        dil = task->linux_task->dilation_factor;

        if (dil == 0)
                temp_proc = 1;
        else if (dil < 0)
                temp_proc = dil*-1;
        if (exp_highest_dilation == dil) {
        		/* if the leaders' TDF and the containers TDF are the same, let it run for the full amount (do not need to scale) */
                task->running_time = FREEZE_QUANTUM;
        }
        else if (exp_highest_dilation > 0 && dil > 0) {
        		/* if both the leaders' TDF and the containers TDF are > 0, scale running time */
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,exp_highest_dilation,&rem) + rem)*dil;
        }
        else if (exp_highest_dilation > 0 && dil == 0) {
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,exp_highest_dilation,&rem) + rem);
		}
        else if (exp_highest_dilation > 0 && dil < 0) { 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION*PRECISION,exp_highest_dilation*temp_proc,&rem) + rem);

        }
        else if (exp_highest_dilation == 0 && dil < 0) { 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM*PRECISION,temp_proc,&rem) + rem);
        }
        else if (exp_highest_dilation < 0 && dil < 0){ 
                task->running_time = (div_s64_rem(FREEZE_QUANTUM,temp_proc,&rem) + rem)*temp_high;
        }
	else {
		PDEBUG_I("Calc Task Runtime: Should be fixed when highest dilation is updated\n");
	}
        return;
}

/***
Function that determines what CPU a particular task should be assigned to. It simply finds the current CPU with the
smallest aggregated running time of all currently assigned containers. All containers that are assigned to the same
CPU are connected as a list, hence I call it a 'chain'
***/
void assign_to_cpu(struct dilation_task_struct* task) {
	int i;
	int index;
	s64 min;
	struct dilation_task_struct *walk;
	index = 0;
	min = chainlength[index];

	for (i=1; i<number_of_heads; i++)
	{
	    if (chainlength[i] < min)
	    {
		    min = chainlength[i];
		    index = i;
	    }
	}

	walk = chainhead[index];
	if (walk == NULL) {
		chainhead[index] = task;
		init_waitqueue_head(&per_cpu_wait_queue[index]);	
		curr_process_finished_flag[index] = 0;			
		atomic_set(&wake_up_signal_sync_drift[index],0);	
	}
	else {

		/* create doubly linked list */
		while (walk->next != NULL)
		{
			walk = walk->next;
		}
		walk->next = task;
		task->prev = walk; 
	}


	/* set CPU mask */
	chainlength[index] = chainlength[index] + task->running_time;
   	bitmap_zero((&task->linux_task->cpus_allowed)->bits, 8);
    cpumask_set_cpu(index+(TOTAL_CPUS - EXP_CPUS),&task->linux_task->cpus_allowed);
	task->cpu_assignment = index+(TOTAL_CPUS - EXP_CPUS);
   	set_children_cpu(task->linux_task, task->cpu_assignment);
	return;
}

/***
Just debug function for containers being mapped to specific CPUs
***/
void printChainInfo() {
    int i;
    s64 max = chainlength[0];
    for (i=0; i<number_of_heads; i++)
    {
    	PDEBUG_I("Print Chain Info: Length of chain %d is %lld\n", i, chainlength[i]);
        if (chainlength[i] > max)
            max = chainlength[i];
    }
}

/***
Get the next task that is allowed to run this round, this means if the running_time > 0 (CBE specific)
***/
struct dilation_task_struct * getNextRunnableTask(struct dilation_task_struct * task) {
    if (task == NULL) {
		return NULL;
	}
	if (task->running_time > 0 && task->stopped != -1)
                return task;
        while (task->next != NULL) {
                task = task->next;
                if (task->running_time > 0 && task->stopped != -1)
                        return task;
    }
    /* if got through loop, this chain is done for this iteration, return NULL */
    return NULL;
}


/***
Set's the past physical times of the process and all it's children to the specified argument 
***/
void set_all_ppp_freeze_times_recurse(struct task_struct * aTask, s64 freeze_time, struct dilation_task_struct * lxc){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;

	if (aTask == NULL) {
		PDEBUG_E("Set all past physical times: Task does not exist\n");
		return;
	}

	if (aTask->pid == 0) {
		return;
	}
	

	me = aTask;
	t = me;
	s64 temp;
	do {
		acquire_irq_lock(&t->dialation_lock,flags);
		t->past_physical_time = lxc->linux_task->past_physical_time;
		t->freeze_time = freeze_time;
		release_irq_lock(&t->dialation_lock,flags);
		
	} while_each_thread(me, t);

	int i = 0;
    list_for_each(list, &aTask->children)
    {

        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        set_all_ppp_freeze_times_recurse(taskRecurse,freeze_time,lxc);
		i++;

    }
}

s64 set_all_process_virtual_times(s64 start_ns){

    struct dilation_task_struct * task = NULL;
    struct list_head *pos;
	struct list_head *n;
	

    if(experiment_stopped == RUNNING){
		list_for_each_safe(pos, n, &exp_list)
		{
			task = list_entry(pos, struct dilation_task_struct, list);
			
			if(task != NULL) {
			
			    if(task->linux_task->freeze_time > 0){
			    
			        s64 temp = task->linux_task->freeze_time;
            	    task->linux_task->freeze_time = task->wake_up_time + task->running_time;
            	    task->linux_task->past_physical_time = task->linux_task->past_physical_time + (task->wake_up_time - temp);
            	}
    		    set_all_ppp_freeze_times_recurse(task->linux_task,task->wake_up_time + task->running_time,task);
    		}
		}
	}

}

/***
Calculate the amount by which the task should advance to reach the expected virtual time
 ***/
s64 calculate_change(struct dilation_task_struct* task, s64 virt_time, s64 expected_time) {
    s64 change;
	s32 rem;
	change = 0;
	unsigned long flags;

	acquire_irq_lock(&task->linux_task->dialation_lock,flags);

	if (expected_time - virt_time < 0)
        {
            if (task->linux_task->dilation_factor > 0)
                    change = div_s64_rem( ((expected_time - virt_time)*-1)*task->linux_task->dilation_factor, PRECISION, &rem);
            else if (task->linux_task->dilation_factor < 0)
            {
                    change = div_s64_rem( ((expected_time - virt_time)*-1)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                    change += rem;
            }
            else if (task->linux_task->dilation_factor == 0)
                    change = (expected_time - virt_time)*-1;
			if (experiment_type == CS) {
        		change *= -1;
			}
        }
        else
        {
            if (task->linux_task->dilation_factor > 0)
                    change = div_s64_rem( (expected_time - virt_time)*task->linux_task->dilation_factor, PRECISION, &rem);
            else if (task->linux_task->dilation_factor < 0)
            {
                    change = div_s64_rem((expected_time - virt_time)*PRECISION, task->linux_task->dilation_factor*-1,&rem);
                    change += rem;
            }
            else if (task->linux_task->dilation_factor == 0)
            {
                    change = (expected_time - virt_time);
            }
			if (experiment_type == CBE) {
            	change *= -1;
			}
        }


	release_irq_lock(&task->linux_task->dialation_lock,flags);
	
	return change;
}



/***
Find the difference between the tasks virtual time and the virtual time it SHOULD be at. Then it
will modify the tasks running_time, to allow it to run either more or less in the next round
***/
void calculate_virtual_time_difference(struct dilation_task_struct* task, s64 now, s64 expected_time)
{
	s64 change;
	ktime_t ktime;
	s64 virt_time;
	s64 change_vt = 0;
	s32 rem;
	s64 diff;

    start_change:
    
	virt_time = get_virtual_time(task, now);
	change = 0;
	change = calculate_change(task, virt_time, expected_time);

	if (experiment_type == CS) {

			/*
	        if (change < 0)
        	{
	        	ktime = ktime_set(0, 0);
	        }
        	else
	        {
        		ktime = ktime_set(0, change);
				
	        }*/
			if(virt_time > expected_time){
	        
	            ktime = ktime_set(0,0);
	        }
	        else{
	        
	           
	           if(task->linux_task->dilation_factor > 0)
    	            diff = div_s64_rem( (expected_time - virt_time)*task->linux_task->dilation_factor, PRECISION, &rem);
	           else
	                diff = expected_time - virt_time;
	                    
	           ktime = ktime_set(0,diff);
	           
	        }

	}
	if (experiment_type == CBE) {
	
	        if(virt_time > expected_time){
	        
	            ktime = ktime_set(0,0);
	        }
	        else{
	        
	           
	           if(task->linux_task->dilation_factor > 0)
    	            diff = div_s64_rem( (expected_time - virt_time)*task->linux_task->dilation_factor, PRECISION, &rem);
	           else
	                diff = expected_time - virt_time;
	                    
	           ktime = ktime_set(0,diff);
	           
	        }

	}
	
    task->stopped = 0;
    //task->running_time = ktime_to_ns(ktime);
    task->running_time = expected_increase;
    return;
}

/***
The function called by each synchronization thread (CBE specific). For every process it is in charge of
it will see how long it should run, then start running the process at the head of the chain.
***/
int calculate_sync_drift(void *data)
{
	int round = 0;
	int cpuID = *((int *)data);
	struct dilation_task_struct *task;
	s64 now;
	struct timeval ktv;
	ktime_t ktime;
	int run_cpu;

	set_current_state(TASK_INTERRUPTIBLE);

	if(atomic_read(&wake_up_signal_sync_drift[cpuID]) != 1)
		atomic_set(&wake_up_signal_sync_drift[cpuID],0);
		
	PDEBUG_I("#### Calculate Sync Drift: Started Sync drift Thread for lxcs on CPU = %d\n",cpuID);

	
	/* if it is the very first round, don't try to do any work, just rest */
	if (round == 0)
		goto startWork;
	
	while (!kthread_should_stop())
	{
		
        if(experiment_stopped == STOPPING) {
        
            set_current_state(TASK_INTERRUPTIBLE);
		    atomic_dec(&worker_count);
		    atomic_set(&wake_up_signal_sync_drift[cpuID],0);
		    run_cpu = get_cpu();   
			PDEBUG_V("#### Calculate Sync Drift: Sending wake up from Sync drift Thread for lxcs on CPU = %d. My Run cpu = %d\n",cpuID,run_cpu);
		    wake_up_interruptible(&wq);
        	return 0;
        }

		task = chainhead[cpuID];
	    

		/* for every task it is responsible for, determine how long it should run */
		while (task != NULL) {
		    do_gettimeofday(&ktv);
    	    now = timeval_to_ns(&ktv);
			calculate_virtual_time_difference(task, now, actual_time);
			task = task->next;
		}

		/* find the first task to run, then run it */
        task = chainhead[cpuID];

		if (task == NULL) {

	
			PDEBUG_V("Calculate Sync Drift: No Tasks in chain %d \n", cpuID);				
			atomic_inc(&running_done);
			atomic_inc(&start_count);
		}
		else {
			do{
		        task = getNextRunnableTask(task);
    	       	if (task != NULL)
    	       	{
					PDEBUG_V("Calculate Sync Drift: Called  UnFreeze Proc Recurse on CPU: %d\n", cpuID);					
    				unfreeze_proc_exp_recurse(task, actual_time);
 					PDEBUG_V("Calculate Sync Drift: Finished Unfreeze Proc on CPU: %d\n", cpuID);
    	           	task = task->next;
				
               	}
               	else
               	{
               		PDEBUG_I("Calculate Sync Drift: %d chain %d has nothing to run\n",round,cpuID);
					break;
               	}
			

			}while(task != NULL);
		}

		PDEBUG_V("Calculate Sync Drift: Thread done with on %d\n",cpuID);
		/* when the first task has started running, signal you are done working, and sleep */
		round++;
		set_current_state(TASK_INTERRUPTIBLE);
		atomic_dec(&worker_count);
		atomic_set(&wake_up_signal_sync_drift[cpuID],0);
		run_cpu = get_cpu();
		PDEBUG_V("#### Calculate Sync Drift: Sending wake up from Sync drift Thread for lxcs on CPU = %d. My Run cpu = %d\n",cpuID,run_cpu);
		wake_up_interruptible(&wq);
		

	startWork:
		schedule();
		set_current_state(TASK_RUNNING);
		run_cpu = get_cpu();
		PDEBUG_V("~~~~ Calculate Sync Drift: I am woken up for lxcs on CPU =  %d. My Run cpu = %d\n",cpuID,run_cpu);
		
	}
	return 0;
}

/***
The main synchronization thread (For CBE mode). When all tasks in a round have completed, this will get
woken up, increment the experiment virtual time,  and then wake up every other synchronization thread 
to have it do work
***/
int catchup_func(void *data)
{
        int round_count;
        struct timeval ktv;
        int i;
        int redo_count;
        round_count = 0;
       
        struct timeval now;
	    s64 start_ns;
	
	 	set_current_state(TASK_INTERRUPTIBLE);
	 	PDEBUG_I("Catchup Func: started.\n");
                
		while (!kthread_should_stop())
        {
            round_count++;
			redo_count = 0;
            redo:

            if (experiment_stopped == STOPPING)
            {

					PDEBUG_I("Catchup Func: Cleaning experiment via catchup task\n");
                    clean_exp();
					set_current_state(TASK_INTERRUPTIBLE);	
					schedule();
					continue;

            }

            actual_time += expected_increase;

			/* clean up any stopped containers, alter TDFs if necessary */
			//clean_stopped_containers();
			if (dilation_change)
				change_containers_dilation();
				
			if(atomic_read(&experiment_stopping) == 1 && atomic_read(&n_active_syscalls) == 0){
				experiment_stopped = STOPPING;
				continue;
			}
			else if(atomic_read(&experiment_stopping) == 1) {
				PDEBUG_I("Catchup Func: Stopping. NActive syscalls = %d\n", atomic_read(&n_active_syscalls));
			}

			if(atomic_read(&progress_cbe_enabled) == 1 && atomic_read(&progress_cbe_rounds) > 0){
				atomic_dec(&progress_cbe_rounds);
				if(atomic_read(&progress_cbe_rounds) == 0) {
					PDEBUG_V("Waking up Progress CBE Process\n");
					wake_up_interruptible(&progress_cbe_wait_queue);
				}
			}

			wait_event_interruptible(progress_cbe_catchup_tsk, (atomic_read(&progress_cbe_enabled) == 1 && atomic_read(&progress_cbe_rounds) > 0) || atomic_read(&progress_cbe_enabled) == 0);
			
            do_gettimeofday(&ktv);
			atomic_set(&start_count, 0);

			/* wait up each synchronization worker thread, then wait til they are all done */
			if (number_of_heads > 0) {

				PDEBUG_V("Catchup Func: Round finished. Waking up worker threads to calculate next round time\n");
				PDEBUG_V("Catchup Func: Current FREEZE_QUANTUM : %d\n", FREEZE_QUANTUM);
				atomic_set(&worker_count, number_of_heads);
			
				for (i=0; i<number_of_heads; i++) {
					curr_sync_task_finished_flag[i] = 1;
					atomic_set(&wake_up_signal_sync_drift[i],1);	
	
					/* chaintask refers to calculate_sync_drift thread */
					if(DEBUG_LEVEL == DEBUG_LEVEL_INFO || DEBUG_LEVEL == DEBUG_LEVEL_VERBOSE) {			
						if(wake_up_process(chaintask[i]) == 1){ 
							PDEBUG_V("Catchup Func: Sync thread %d wake up\n",i);
						}
						else{
						    while(wake_up_process(chaintask[i]) != 1);
							PDEBUG_V("Catchup Func: Sync thread %d already running\n",i);
						}
					}
				}

				int run_cpu;
				run_cpu = get_cpu();
				PDEBUG_V("Catchup Func: Waiting for sync drift threads to finish. Run_cpu %d\n",run_cpu);
            
                do_gettimeofday(&now);
                start_ns = timeval_to_ns(&now);
    
				wait_event_interruptible(wq, atomic_read(&worker_count) == 0);
				set_current_state(TASK_INTERRUPTIBLE);

				PDEBUG_V("Catchup Func: All sync drift thread finished\n");	
				if(experiment_type != CS) {
					#ifdef MULTI_CORE_NODES
						set_all_process_virtual_times(start_ns);		
					#endif
				}
			}

			/* if there are no continers in the experiment, then stop the experiment */
			if (proc_num == 0 && experiment_stopped == RUNNING) {
				PDEBUG_I("Catchup Func: Cleaning experiment via catchup task because no tasks left\n");
           		clean_exp();	
				set_current_state(TASK_INTERRUPTIBLE);	
				schedule();
				continue;
	  		
			}
		    else if ((number_of_heads > 0 && atomic_read(&start_count) == number_of_heads) || (number_of_heads > 0 && atomic_read(&running_done) == number_of_heads))
		    { 	/* something bad happened, because not a single task was started, think due to page fault */
			    redo_count++;
			    atomic_set(&running_done, 0);
			    PDEBUG_I("Catchup Func: %d Redo computations %d Proc_num: %d Exp stopped: %d\n",round_count, redo_count, proc_num, experiment_stopped);
			    goto redo;
		    }

			end:
                set_current_state(TASK_INTERRUPTIBLE);	
		
			if(experiment_stopped == NOTRUNNING){
				PDEBUG_I("Catchup Func: Waiting to be woken up\n");
				set_current_state(TASK_INTERRUPTIBLE);	
				schedule();			
			}
			PDEBUG_V("Catchup Func: Resumed\n");
        }
        return 0;
}

/***
If a LXC had its TDF changed during an experiment, modify the experiment accordingly (ie, make it the
new leader, and so forth)
***/
void change_containers_dilation() {
        struct list_head *pos;
        struct list_head *n;
        struct dilation_task_struct* task;
        struct dilation_task_struct* possible_leader;
        int new_highest;
		unsigned long flags;

        new_highest = -99999;
        possible_leader = NULL;

        list_for_each_safe(pos, n, &exp_list)
        {
            task = list_entry(pos, struct dilation_task_struct, list);

			/* its stopped, so skip it */
            if (task->stopped  == -1) 
            {
				continue;
            }
			if (task->newDilation != -1) {

				/* change its dilation */
				dilate_proc_recurse_exp(task->linux_task->pid, task->newDilation); 
				task->newDilation = -1; 
			
				/* update its runtime */
				calcTaskRuntime(task);  
			}

			acquire_irq_lock(&task->linux_task->dialation_lock,flags);
            if (task->linux_task->dilation_factor > new_highest)
            {
                new_highest = task->linux_task->dilation_factor;
                possible_leader = task;
            }
			release_irq_lock(&task->linux_task->dialation_lock,flags);

        }

		if (new_highest > exp_highest_dilation || new_highest < exp_highest_dilation)
	    {
	        /* If we have a new highest dilation, or if the leader container finished - save the new leader  */
            exp_highest_dilation = new_highest;
            leader_task = possible_leader;
            if (leader_task != NULL)
            {
                calcExpectedIncrease();
		        PDEBUG_I("Change Containers Dilation: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
            }
            /* update running time for each container because we have a new leader */
            list_for_each_safe(pos, n, &exp_list)
            {
                task = list_entry(pos, struct dilation_task_struct, list);
                calcTaskRuntime(task);
            }
        }

		/* reset global flag */
		dilation_change = 0; 
}

/***
If a container stops in the middle of an experiment, clean it up
***/
void clean_stopped_containers() {
    struct list_head *pos;
    struct list_head *n;
    struct dilation_task_struct* task;
    struct dilation_task_struct* next_task;
    struct dilation_task_struct* prev_task;
    struct dilation_task_struct* possible_leader;
    int new_highest;
    int did_leader_finish;
	unsigned long flags;

    did_leader_finish = 0;
    new_highest = -99999;
    possible_leader = NULL;

	/* for every container in the experiment, see if it has finished execution, if yes, clean it up.
	also check the possibility of needing to determine a new leader */
    list_for_each_safe(pos, n, &exp_list)
    {
		task = list_entry(pos, struct dilation_task_struct, list);
        if (task->stopped  == -1) //if stopped = -1, the process is no longer running, so free the structure
        {

			PDEBUG_I("Clean Stopped Containers: Detected stopped task\n");
          	if (leader_task == task)
			{ 
				/* the leader task is done, we NEED a new leader */
           		did_leader_finish = 1;
           	}
           	if (&task->timer != NULL)
           	{
           		hrtimer_cancel( &task->timer );
           	}

			prev_task = task->prev;
			next_task = task->next;

			/* handle head/tail logic */
			if (prev_task == NULL && next_task == NULL) {
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = NULL;
				PDEBUG_I("Clean Stopped Containers: Stopping only head task for cPUID %d\n", task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS));
			}
			else if (prev_task == NULL) { 
				/* the stopped task was the head */
				chainhead[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] = next_task;
				next_task->prev = NULL;
			}
			else if (next_task == NULL) { //the stopped task was the tail
				prev_task->next = NULL;
			}
			else { 
				/* somewhere in the middle */
				prev_task->next = next_task;
				next_task->prev = prev_task;
			}

			chainlength[task->cpu_assignment - (TOTAL_CPUS - EXP_CPUS)] -= task->running_time;

			proc_num--;
           	PDEBUG_I("Clean Stopped Containers: Process %d is stopped!\n", task->linux_task->pid);
			list_del(pos);
           	kfree(task);
			continue;
        }

		acquire_irq_lock(&task->linux_task->dialation_lock,flags);
        if (task->linux_task->dilation_factor > new_highest)
        {
        	new_highest = task->linux_task->dilation_factor;
            possible_leader = task;
        }
		release_irq_lock(&task->linux_task->dialation_lock,flags);

	}
    if (new_highest > exp_highest_dilation || did_leader_finish == 1)
    {   
		/* If we have a new highest dilation, or if the leader container finished - save the new leader */
    	exp_highest_dilation = new_highest;
        leader_task = possible_leader;
        if (leader_task != NULL)
        {
           	calcExpectedIncrease();
			PDEBUG_I("Clean Stopped Containers: New highest dilation is: %d new expected_increase: %lld\n", exp_highest_dilation, expected_increase);
        }

        /* update running time for each container because we have a new leader */
        list_for_each_safe(pos, n, &exp_list)
        {
           	task = list_entry(pos, struct dilation_task_struct, list);
            calcTaskRuntime(task);
        }
    }
	stopped_change = 0;
    return;
}

/***
What gets called when a containers hrtimer interrupt occurs: the task is frozen, then it determines the next container that
should be ran within that round.
***/
enum hrtimer_restart exp_hrtimer_callback( struct hrtimer *timer )
{
	int dil;
	struct dilation_task_struct *task;
	struct dilation_task_struct * callingtask;
	struct timeval tv;
	s64 now;
	int startJob;
	ktime_t ktime;
	

	do_gettimeofday(&tv);
	now = timeval_to_ns(&tv);

	task = container_of(timer, struct dilation_task_struct, timer);
	dil = task->linux_task->dilation_factor;
	callingtask = task;
	int CPUID = callingtask->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);

	
	#ifndef MULTI_CORE_NODES
	if(task->last_run != NULL){
		struct task_struct * last_task = task->last_run->curr_task;
		if(last_task != NULL && find_task_by_pid(task->last_run->pid) != NULL) {
			last_task->freeze_time = task->last_timer_fire_time + task->last_timer_duration;
		}	
	}
	#endif
	
	

	if (catchup_task == NULL) {
		PDEBUG_E("Hrtimer Callback: Proc called but catchup_task is null\n");
		return HRTIMER_NORESTART;
	}

	/* if the process is done, dont bother freezing it, just set flag so it gets cleaned in sync phase */
	if (callingtask->stopped == -1) {
		stopped_change = 1;
		curr_process_finished_flag[CPUID] = 1;
		atomic_set(&wake_up_signal[CPUID], 1);
		wake_up(&per_cpu_wait_queue[CPUID]);
		wake_up_process(chaintask[CPUID]);

	}
	else { 
		/* its not done, so freeze */
		task->stopped = 1;
		curr_process_finished_flag[CPUID] = 1;
		atomic_set(&wake_up_signal[CPUID], 1);
		wake_up(&per_cpu_wait_queue[CPUID]);
		wake_up_process(chaintask[CPUID]);		
	
	}

    return HRTIMER_NORESTART;
}
/***
Set's the freeze times of the process and all it's children to the specified argument 
***/
void set_all_freeze_times_recurse(struct task_struct * aTask, s64 freeze_time,s64 last_ppp, int max_no_recursions){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;
	s32 rem;

	if(max_no_recursions >= 100)
		return;

    if (aTask == NULL) {

       	PDEBUG_E("Set all freeze times: Task does not exist\n");
        return;
    }
    if (aTask->pid == 0) {

		PDEBUG_E("Set all freeze times: pid 0 error\n");
        return;
    }

	me = aTask;
	t = me;
	do {

		acquire_irq_lock(&t->dialation_lock,flags);
		t->freeze_time = freeze_time; 
		kill(t,SIGSTOP,NULL);
		release_irq_lock(&t->dialation_lock,flags);

	} while_each_thread(me, t);

	int i = 0;
    list_for_each(list, &aTask->children)
    {
		if(i > 1000)
			return;
        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        set_all_freeze_times_recurse(taskRecurse, freeze_time,last_ppp,max_no_recursions + 1);
		i++;

    }
}


/***
Set's the past physical times of the process and all it's children to the specified argument 
***/
void set_all_past_physical_times_recurse(struct task_struct * aTask, s64 time, int max_no_of_recursions, struct dilation_task_struct * lxc){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	unsigned long flags;

	if(max_no_of_recursions >= 100)
		return;

	if (aTask == NULL) {
		PDEBUG_E("Set all past physical times: Task does not exist\n");
		return;
	}

	if (aTask->pid == 0) {
		PDEBUG_E("Set all past physical times: pid 0 error\n");
		return;
	}
	
	if(lxc->linux_task->freeze_time > 0){
	    lxc->linux_task->past_physical_time = lxc->linux_task->past_physical_time + (time - lxc->linux_task->freeze_time);
	    lxc->linux_task->freeze_time = 0;
	}

	me = aTask;
	t = me;
	do {
	
		if(t->pid != lxc->linux_task->pid){
			acquire_irq_lock(&t->dialation_lock,flags);
			if (t->freeze_time > 0)
		   	{
				t->past_physical_time = lxc->linux_task->past_physical_time;
				t->freeze_time = 0;
		   	}
		   	else{
		   	    PDEBUG_I("Set all past physical times: freeze_time not set pid = %d\n", t->pid);
		   	    t->past_physical_time = lxc->linux_task->past_physical_time;
	  	    	t->freeze_time = 0;
		   	}
			t->virt_start_time = lxc->linux_task->virt_start_time;
			t->past_virtual_time = lxc->linux_task->past_virtual_time;
			release_irq_lock(&t->dialation_lock,flags);
		}
		
	} while_each_thread(me, t);

	int i = 0;
    list_for_each(list, &aTask->children)
    {
		if(i > 1000)
			return;

        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
                continue;
        }

        set_all_past_physical_times_recurse(taskRecurse, time,max_no_of_recursions + 1,lxc);
		i++;

    }
}

/***
Searches an lxc for the process with given pid. returns success if found. Max no of recursiond is just for safety
***/

struct task_struct * search_lxc(struct task_struct * aTask, int pid, int max_no_of_recursions){


	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;

	if(max_no_of_recursions >= 100)
		return NULL;

	if (aTask == NULL) {
		PDEBUG_E("Search lxc: Task does not exist\n");
		return NULL;
	}

	if (aTask->pid == 0) {
		PDEBUG_E("Search lxc: pid 0 error\n");
		return NULL;
	}

	me = aTask;
	t = me;

	int i = 0;
	if(t->pid == pid)
		return t;

	do {
		if (t->pid == pid)
      	{
			return t;			
       	}
	} while_each_thread(me, t);

	list_for_each(list, &aTask->children)
	{
		if(i > 1000)
			return NULL;

		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
			continue;
		}
		t =  search_lxc(taskRecurse, pid, max_no_of_recursions + 1);
		if(t != NULL)
			return t;
		i++;
	}

	return NULL;
}


/***
Actually cleans up the experiment by freeing all memory associated with the every container
***/
void clean_exp() {
	struct list_head *pos;
	struct list_head *n;
	struct dilation_task_struct *task;
	struct list_head *list_2;
    	struct task_struct *taskRecurse;
	int ret;
	int i;
	struct sched_param sp;
	struct timeline* curr;
	struct timeline* tmp;
	struct timeval now;
	s64 now_ns;
	unsigned long flags;

	PDEBUG_A("Clean Exp: Starting Experiment Cleanup\n");
	ret = 0;
	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);

	if(experiment_type != CS)
		set_current_state(TASK_INTERRUPTIBLE);	

	if(experiment_type != NOTSET){

		PDEBUG_A("Clean Exp: Resetting Sys Call table\n");
		orig_cr0 = read_cr0();
		write_cr0(orig_cr0 & ~0x00010000);
		sys_call_table[__NR_poll] = (unsigned long *)ref_sys_poll;	
		sys_call_table[NR_select] = (unsigned long *)ref_sys_select;
		sys_call_table[__NR_nanosleep] = (unsigned long *)ref_sys_sleep;
		sys_call_table[__NR_clock_gettime] = (unsigned long *) ref_sys_clock_gettime;
		sys_call_table[__NR_clock_nanosleep] = (unsigned long *) ref_sys_clock_nanosleep;
		write_cr0(orig_cr0 | 0x00010000);
		PDEBUG_A("Clean Exp: Sys Call table Updated\n");
	}
   

	/* free any heap memory associated with each container, cancel corresponding timers */
    list_for_each_safe(pos, n, &exp_list)
    {
        	task = list_entry(pos, struct dilation_task_struct, list);
		sp.sched_priority = 0;
		if (experiment_stopped != NOTRUNNING) {
			
			if(experiment_type == CS)			
				resume_all(task->linux_task,task);		
			
					
			if (experiment_type == CS || experiment_type == CBE){
	
				acquire_irq_lock(&task->linux_task->dialation_lock,flags);
				task->linux_task->past_physical_time = task->linux_task->past_physical_time + (now_ns - task->linux_task->freeze_time);
				task->linux_task->past_physical_time = 0;
				task->linux_task->freeze_time = 0;
				task->linux_task->virt_start_time = 0;
				release_irq_lock(&task->linux_task->dialation_lock,flags);
				kill(task->linux_task,SIGCONT,NULL);
				unfreeze_all(task->linux_task);

			}
			

		    	if (task->stopped != -1 && ret != -1) {
		        	sp.sched_priority = 0;
		        	if (sched_setscheduler(task->linux_task, SCHED_NORMAL, &sp) == -1 )
		        	        PDEBUG_A("Clean Exp: Error setting policy: %d pid: %d\n", SCHED_NORMAL, task->linux_task->pid);
		                    
		 			set_children_policy(task->linux_task, SCHED_NORMAL, 0);
					cpumask_setall(&task->linux_task->cpus_allowed);
	
					/* -1 to fill cpu mask so that they can be scheduled in any cpu */
					set_children_cpu(task->linux_task, -1);
			}
			
        	}

		list_del(pos);
	        if (&task->timer != NULL)
        	{
        	        ret = hrtimer_cancel( &task->timer );
        	        if (ret) PDEBUG_A("Clean Exp: The timer was still in use...\n");
        	}
		clean_up_schedule_list(task);
		kfree(task);
	}

    PDEBUG_A("Clean Exp: Linked list deleted\n");
    for (i=0; i<number_of_heads; i++) //clean up cpu specific chains
    {
		chainhead[i] = NULL;
		chainlength[i] = 0;
		if (experiment_stopped != NOTRUNNING) {
			PDEBUG_A("Clean Exp: Stopping chaintask %d\n", i);
			if (chaintask[i] != NULL && kthread_stop(chaintask[i]) )
			{
		        		PDEBUG_A("Clean Exp: Stopping worker %d error\n", i);
			}
			PDEBUG_A("Clean Exp: Stopped chaintask %d\n", i);
		}

		/* clean up timeline structs */
   		if (experiment_type == CS) {

			curr = timelineHead[i];
			timelineHead[i] = NULL;
			tmp = curr;
			while (curr != NULL) {
				tmp = curr;

				/* move to next timeline */
				curr = curr->next; 
				atomic_set(&tmp->pthread_done,1);
				atomic_set(&tmp->progress_thread_done,1);
				atomic_set(&tmp->stop_thread,2);
				wake_up_interruptible_sync(&tmp->pthread_queue);	
				wake_up_interruptible_sync(&tmp->progress_thread_queue);				
				PDEBUG_A("Clean Exp: Stopped all kernel threads for timeline : %d\n",tmp->number);
			}
		}
	}

	

	proc_num = 0;

	/* reset highest_dilation */
	exp_highest_dilation = -100000000; 
	leader_task = NULL;
	atomic_set(&running_done, 0);
	atomic_set(&experiment_stopping,0);
	number_of_heads = 0;
	stopped_change = 0;


	if(experiment_stopped != NOTRUNNING && experiment_type == CBE) {
		wake_up_interruptible(&cbe_exp_stop_queue);
	}	
	experiment_stopped = NOTRUNNING;
	experiment_type = NOTSET;
	
	PDEBUG_A("Clean Exp: Exited Clean Experiment\n");
	
	
	

	

}

/***
Sets the experiment_stopped flag, signalling catchup_task (sync function) to stop at the end of the current round and clean up
***/
void set_clean_exp() {

		int ret = 0;

		/* assuming stopExperiment will not be called if still waiting for a S3F progress to return */
		if (experiment_stopped == NOTRUNNING || experiment_type == CS || experiment_stopped == FROZEN) {

		    /* sync experiment was never started, so just clean the list */
			PDEBUG_A("Set Clean Exp: Clean up immediately..\n");
			experiment_stopped = STOPPING;
			atomic_set(&experiment_stopping,1);
		    	clean_exp();
		}
		else if (experiment_stopped == RUNNING) {

			/* the experiment is running, so set the flag */
			PDEBUG_A("Set Clean Exp: Waiting for catchup task to run before cleanup\n");
			set_current_state(TASK_INTERRUPTIBLE);
			atomic_set(&experiment_stopping,1);
			
			do{
				ret = wait_event_interruptible_timeout(cbe_exp_stop_queue,atomic_read(&experiment_stopping) == 0,HZ);
				if(ret == 0)
					set_current_state(TASK_INTERRUPTIBLE);
				else
					set_current_state(TASK_RUNNING);

			}while(ret == 0);			
			
			PDEBUG_A("Set Clean Exp: Cleanup Complete\n");
		}

		PDEBUG_A("Set Clean Exp: Exiting ..\n");
		kill(current,SIGCONT,NULL);

        return;
}

/***
Set the time dilation variables to be consistent with all children
***/
void set_children_time(struct task_struct *aTask, s64 time) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;

	if (aTask == NULL) {
        PDEBUG_E("Set Children Time: Task does not exist\n");
        return;
    }
    if (aTask->pid == 0) {
        return;
    }
	me = aTask;
	t = me;

	/* set it for all threads */
	do {
		acquire_irq_lock(&t->dialation_lock,flags);
		if (t->pid != aTask->pid) {
           		t->virt_start_time = time;
           		t->freeze_time = time;
           		t->past_physical_time = 0;
           		t->past_virtual_time = 0;
			if(experiment_stopped != RUNNING)
     	       	t->wakeup_time = 0;
		}
		release_irq_lock(&t->dialation_lock,flags);
		
	} while_each_thread(me, t);


	list_for_each(list, &aTask->children)
	{
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
		        return;
		}
		acquire_irq_lock(&taskRecurse->dialation_lock,flags);
		taskRecurse->virt_start_time = time;
		taskRecurse->freeze_time = time;
		taskRecurse->past_physical_time = 0;
		taskRecurse->past_virtual_time = 0;
		if(experiment_stopped != RUNNING)
		    taskRecurse->wakeup_time = 0;
		release_irq_lock(&taskRecurse->dialation_lock,flags);
		set_children_time(taskRecurse, time);
	}
}

/***
Set the time dilation variables to be consistent with all children
***/
void set_children_policy(struct task_struct *aTask, int policy, int priority) {
        struct list_head *list;
        struct task_struct *taskRecurse;
        struct sched_param sp;
        struct task_struct *me;
        struct task_struct *t;

		if (aTask == NULL) {
		   	PDEBUG_E("Set Children Policy: Task does not exist\n");
		    return;
		}
		if (aTask->pid == 0) {
		    return;
		}

		me = aTask;
		t = me;

		/* set policy for all threads as well */
		do {
			if (t->pid != aTask->pid) {
		   		sp.sched_priority = priority; 
				if (sched_setscheduler(t, policy, &sp) == -1 )
		    	    PDEBUG_I("Set Children Policy: Error setting thread policy: %d pid: %d\n",policy,t->pid);
			}

		} while_each_thread(me, t);

		list_for_each(list, &aTask->children)
		{
	        taskRecurse = list_entry(list, struct task_struct, sibling);
	        if (taskRecurse->pid == 0) {
	                return;
	        }

			/* set children scheduling policy */
		    sp.sched_priority = priority; 
			if (sched_setscheduler(taskRecurse, policy, &sp) == -1 )
		        PDEBUG_I("Set Children Policy: Error setting policy: %d pid: %d\n",policy,taskRecurse->pid);
		    set_children_policy(taskRecurse, policy, priority);
		}
}

/***
Set the time dilation variables to be consistent with all children
***/
void set_children_cpu(struct task_struct *aTask, int cpu) {
	struct list_head *list;
	struct task_struct *taskRecurse;
	struct task_struct *me;
    	struct task_struct *t;

	if (aTask == NULL) {
		PDEBUG_E("Set Children CPU: Task does not exist\n");
		return;
	}
		if (aTask->pid == 0) {
			return;
	}

	me = aTask;
	t = me;

	/* set policy for all threads as well */
	do {
		if (t->pid != aTask->pid) {
			if (cpu == -1) {

				/* allow all cpus */
				cpumask_setall(&t->cpus_allowed);
			}
			else {
				bitmap_zero((&t->cpus_allowed)->bits, 8);
       				cpumask_set_cpu(cpu,&t->cpus_allowed);
			}
		}
	} while_each_thread(me, t);

	list_for_each(list, &aTask->children)
	{
	taskRecurse = list_entry(list, struct task_struct, sibling);
	if (taskRecurse->pid == 0) {
		return;
	}
		if (cpu == -1) {

			/* allow all cpus */
			cpumask_setall(&taskRecurse->cpus_allowed);
	    }
		else {
			bitmap_zero((&taskRecurse->cpus_allowed)->bits, 8);
		cpumask_set_cpu(cpu,&taskRecurse->cpus_allowed);
	}
		set_children_cpu(taskRecurse, cpu);
	}
}


/***
Freezes all children associated with a container
***/
int kill_children(struct task_struct *aTask) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct dilation_task_struct *dilTask;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;
	

    if (aTask == NULL) {
    
       	PDEBUG_E("Kill Children: Task does not exist\n");
        return 0;
    }
    if (aTask->pid == 0) {

		PDEBUG_E("Kill Children: PID equals 0\n");
        return 0;
    }

    list_for_each(list, &aTask->children)
    {
            taskRecurse = list_entry(list, struct task_struct, sibling);
            if(taskRecurse == aTask || taskRecurse->pid == aTask->pid)
            	return 0;
            	
            if (taskRecurse->pid == 0) {
                    continue;
            }
		    
			/* just in case - to stop all threads */
			kill(taskRecurse, SIGKILL, NULL); 
		
         
    }
	return 0;
}

/***
Freezes all children associated with a container
***/
int freeze_children(struct task_struct *aTask, s64 time) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct dilation_task_struct *dilTask;
    struct task_struct *me;
    struct task_struct *t;
	unsigned long flags;
	

    if (aTask == NULL) {
       	PDEBUG_E("Freeze Children: Task does not exist\n");
        return 0;
    }
    if (aTask->pid == 0) {

		PDEBUG_E("Freeze Children: PID equals 0\n");
        return 0;
    }

	me = aTask;
	t = me;
	do {
		if (t->pid != aTask->pid) {
			acquire_irq_lock(&t->dialation_lock,flags);
            t->freeze_time = time;
       		release_irq_lock(&t->dialation_lock,flags);
		 	/* to stop any threads */
			kill(t, SIGSTOP, NULL); 
		}
	} while_each_thread(me, t);

    list_for_each(list, &aTask->children)
    {
            taskRecurse = list_entry(list, struct task_struct, sibling);
            if(taskRecurse == aTask || taskRecurse->pid == aTask->pid)
            	return 0;
            	
            if (taskRecurse->pid == 0) {
                    continue;
            }

			acquire_irq_lock(&taskRecurse->dialation_lock,flags);			
		    taskRecurse->freeze_time = time;
			release_irq_lock(&taskRecurse->dialation_lock,flags);

			/* just in case - to stop all threads */
			kill(taskRecurse, SIGSTOP, NULL); 
		
            if (freeze_children(taskRecurse, time) == -1)
		         return 0;
    }
	return 0;
}

/***
Freezes the container, then calls freeze_children to freeze all of the children
***/
int freeze_proc_exp_recurse(struct dilation_task_struct *aTask) {
	struct timeval ktv;
	s64 now;
	unsigned long flags;

	do_gettimeofday(&ktv);
	now = (timeval_to_ns(&ktv));

	acquire_irq_lock(&aTask->linux_task->dialation_lock,flags);
    if(aTask->linux_task->freeze_time == 0)  
		aTask->linux_task->freeze_time = now;
	release_irq_lock(&aTask->linux_task->dialation_lock,flags);

	kill(aTask->linux_task, SIGSTOP, aTask);
    freeze_children(aTask->linux_task, aTask->linux_task->freeze_time);
    return 0;
}


/***
Unfreezes all children associated with a container
***/
int unfreeze_children(struct task_struct *aTask, s64 time, s64 expected_time,struct dilation_task_struct * lxc) {
	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
	unsigned long flags;

	if (aTask == NULL) {
		PDEBUG_E("Unfreeze Children: Task does not exist\n");
		return 0;
	}
	if (aTask->pid == 0) {

		PDEBUG_E("Unfreeze Children: PID equals 0\n");
		return 0;
	}

	me = aTask;
	t = me;
	do {
		acquire_irq_lock(&t->dialation_lock,flags);

		if(experiment_stopped == STOPPING){
			t->virt_start_time = 0;
		}
		
		if (t->pid == aTask->pid) {
			release_irq_lock(&t->dialation_lock,flags);
		}
		else {
			if (t->freeze_time > 0)
			{
				t->past_physical_time = t->past_physical_time + (time - t->freeze_time);
				t->freeze_time = 0;
				kill(t, SIGCONT, NULL);
			}
			else {

				t->past_physical_time = aTask->past_physical_time;

				/* just in case - to continue all threads */
				kill(t, SIGCONT, NULL); 
				PDEBUG_V("Unfreeze Children: Thread not Frozen. Pid: %d Dilation %d\n", t->pid, t->dilation_factor);
			}
			
			task_poll_helper = hmap_get_abs(&poll_process_lookup,t->pid);
			task_select_helper = hmap_get_abs(&select_process_lookup,t->pid);
			task_sleep_helper = hmap_get_abs(&sleep_process_lookup,t->pid);

			
			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){
				
				release_irq_lock(&t->dialation_lock,flags);
				kill(t, SIGCONT, dilTask);

            }
            else {
				
				if(task_poll_helper != NULL){				
					
					atomic_set(&task_poll_helper->done,1);
					wake_up(&task_poll_helper->w_queue);
					release_irq_lock(&t->dialation_lock,flags);
					kill(t, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					

					atomic_set(&task_select_helper->done,1);
					wake_up(&task_select_helper->w_queue);
					release_irq_lock(&t->dialation_lock,flags);
					kill(t, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				

					atomic_set(&task_sleep_helper->done,1);
					wake_up(&task_sleep_helper->w_queue);
					release_irq_lock(&t->dialation_lock,flags);
					kill(t, SIGCONT, NULL);

				}
				else {
					release_irq_lock(&t->dialation_lock,flags);

				}
                
 			}


		}
		
		

	} while_each_thread(me, t);

    list_for_each(list, &aTask->children)
    {
        taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
            continue;
        }
		dilTask = container_of(&taskRecurse, struct dilation_task_struct, linux_task);
		

		if(experiment_stopped == STOPPING)
			taskRecurse->virt_start_time = 0;

		acquire_irq_lock(&taskRecurse->dialation_lock,flags);
		if (taskRecurse->wakeup_time != 0 && expected_time > taskRecurse->wakeup_time) {
			
			
			PDEBUG_V("Unfreeze Children: PID : %d Time to wake up: %lld actual time: %lld\n", taskRecurse->pid, taskRecurse->wakeup_time, expected_time);
			taskRecurse->virt_start_time = aTask->virt_start_time;
            taskRecurse->freeze_time = 0;
			taskRecurse->past_physical_time = aTask->past_physical_time;
			taskRecurse->past_virtual_time = aTask->past_virtual_time;
			taskRecurse->wakeup_time = 0;
			

			task_sleep_helper = hmap_get_abs(&sleep_process_lookup,taskRecurse->pid);
			if(task_sleep_helper != NULL) {
				atomic_set(&task_sleep_helper->done,1);
				wake_up(&task_sleep_helper->w_queue);
			}
			
			release_irq_lock(&taskRecurse->dialation_lock,flags);
			/* just in case - to continue all threads */
			kill(taskRecurse, SIGCONT, dilTask); 
			
		}
		else if (taskRecurse->wakeup_time != 0 && expected_time < taskRecurse->wakeup_time) {
			taskRecurse->freeze_time = 0; 
			taskRecurse->past_physical_time = aTask->past_physical_time; // *** trying
			release_irq_lock(&taskRecurse->dialation_lock,flags);
			
		}
		else if (taskRecurse->freeze_time > 0)
		{
			task_poll_helper = hmap_get_abs(&poll_process_lookup,taskRecurse->pid);
			task_select_helper = hmap_get_abs(&select_process_lookup,taskRecurse->pid);
			task_sleep_helper = hmap_get_abs(&sleep_process_lookup,taskRecurse->pid);

			taskRecurse->past_physical_time = taskRecurse->past_physical_time + (time - taskRecurse->freeze_time);
			taskRecurse->freeze_time = 0;
			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){
				
				release_irq_lock(&taskRecurse->dialation_lock,flags);
				kill(taskRecurse, SIGCONT, dilTask);

            }
            else {
				
				if(task_poll_helper != NULL){				
					

					atomic_set(&task_poll_helper->done,1);
					wake_up(&task_poll_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					
					

					atomic_set(&task_select_helper->done,1);
					wake_up(&task_select_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				
					
					atomic_set(&task_sleep_helper->done,1);
					wake_up(&task_sleep_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else{
					release_irq_lock(&taskRecurse->dialation_lock,flags);

				}	
                
 			}
                
        }
		else {
			task_poll_helper = hmap_get_abs(&poll_process_lookup,taskRecurse->pid);
			task_select_helper = hmap_get_abs(&select_process_lookup,taskRecurse->pid);
			task_sleep_helper = hmap_get_abs(&sleep_process_lookup,taskRecurse->pid);

			taskRecurse->past_physical_time = aTask->past_physical_time; // *** trying

			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){
       			PDEBUG_V("Unfreeze Children: Process not frozen. Pid: %d Dilation %d\n", taskRecurse->pid, taskRecurse->dilation_factor);
				release_irq_lock(&taskRecurse->dialation_lock,flags);
				kill(taskRecurse, SIGCONT, dilTask);		
			}
			else {

				if(task_poll_helper != NULL){				
					
					atomic_set(&task_poll_helper->done,1);
					wake_up(&task_poll_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else if(task_select_helper != NULL){					
					

					atomic_set(&task_select_helper->done,1);
					wake_up(&task_select_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);
				}
				else if( task_sleep_helper != NULL) {				

					atomic_set(&task_sleep_helper->done,1);
					wake_up(&task_sleep_helper->w_queue);
					release_irq_lock(&taskRecurse->dialation_lock,flags);
					kill(taskRecurse, SIGCONT, NULL);

				}
				else{
					release_irq_lock(&taskRecurse->dialation_lock,flags);
				}

			}
			
			/* no need to unfreeze its children	*/	
            return -1;					
		}
        if (unfreeze_children(taskRecurse, time, expected_time,lxc) == -1)
			return 0;
        }
	return 0;
}



/***
Resumes all children associated with a container. Called on cleanup
***/
int resume_all(struct task_struct *aTask,struct dilation_task_struct * lxc) {
    struct list_head *list;
    struct task_struct *taskRecurse;
    struct dilation_task_struct *dilTask;
    struct task_struct *me;
    struct task_struct *t;
    struct poll_helper_struct * helper;
    struct select_helper_struct * sel_helper;
    struct sleep_helper_struct * sleep_helper;
	unsigned long flags;

	if (aTask == NULL) {
		PDEBUG_E("Resume All: Task does not exist\n");
		return 0;
	}

	if (aTask->pid == 0) {
		PDEBUG_E("Resume All: PID equals 0\n");
		return 0;
	}

	me = aTask;
	t = me;
	do {

		acquire_irq_lock(&t->dialation_lock,flags);
		helper = hmap_get_abs(&poll_process_lookup,t->pid);
		sel_helper = hmap_get_abs(&select_process_lookup,t->pid);
		sleep_helper = hmap_get_abs(&sleep_process_lookup,t->pid);
		if(helper != NULL){
			t->wakeup_time = 0;
			t->freeze_time = 0;
			t->virt_start_time = 0;
			t->past_physical_time = 0;
			t->past_virtual_time = 0;
			helper->err = FINISHED;
			atomic_set(&helper->done,1);			
			release_irq_lock(&t->dialation_lock,flags);
			wake_up(&helper->w_queue);
			
		}
		else if(sel_helper != NULL){

			t->wakeup_time = 0;
			t->freeze_time = 0;
			t->virt_start_time = 0;
			t->past_physical_time = 0;
			t->past_virtual_time = 0;
			sel_helper->ret = FINISHED;
			atomic_set(&sel_helper->done,1);
			release_irq_lock(&t->dialation_lock,flags);
			wake_up(&sel_helper->w_queue);
			
			
		}
		else if (sleep_helper != NULL) {
			t->wakeup_time = 0;
			t->freeze_time = 0;
			t->virt_start_time = 0;
			t->past_physical_time = 0;
			t->past_virtual_time = 0;
			atomic_set(&sleep_helper->done,1);
			release_irq_lock(&t->dialation_lock,flags);
			wake_up(&sleep_helper->w_queue);
			
		}
		else {
		
			t->virt_start_time = 0;
			t->past_physical_time = 0;
			t->past_virtual_time = 0;
			t->freeze_time = 0;
			t->wakeup_time = 0;
			release_irq_lock(&t->dialation_lock,flags);
			
		}

	} while_each_thread(me, t);

	
    list_for_each(list, &aTask->children)
    {
		taskRecurse = list_entry(list, struct task_struct, sibling);
        if (taskRecurse->pid == 0) {
            continue;
        }
	    resume_all(taskRecurse,lxc);
    }
	return 0;
}



/***
Get next task to run from the run queue of the lxc
***/
lxc_schedule_elem * get_next_valid_task(struct dilation_task_struct * lxc, s64 expected_time){

	struct pid *pid_struct;
	struct task_struct *task;
	int count = 0;
	struct poll_helper_struct * task_poll_helper;
	struct select_helper_struct * task_select_helper;
	int ret = 0;
	unsigned long flags;
 
 
 	
	lxc_schedule_elem * head = schedule_list_get_head(lxc);
	
	redo_again:
	if(head == NULL) {
		PDEBUG_I("Get next valid task: Head is Null. Pid = %d. Size = %d \n", lxc->linux_task->pid, schedule_list_size(lxc));
		lxc->stopped = -1;
		return NULL;
	}

	do{

		/* function to find the pid_struct */
		pid_struct = find_get_pid(head->pid); 	 
		task = pid_task(pid_struct,PIDTYPE_PID); 
		task = search_lxc(lxc->linux_task,head->pid,0);

		if(task == NULL){	
			/* task is no longer running. remove from schedule queue */
			PDEBUG_I("Get Next Valid Task: Task %d no longer running. Removing from schedule queue\n",head->pid);
			PDEBUG_V("Get Next Valid Task: Schedule List Before\n");
			print_schedule_list(lxc);
			pop_schedule_list(lxc);
			head = schedule_list_get_head(lxc);
			PDEBUG_V("Get Next Valid Task: Schedule List After\n");
			print_schedule_list(lxc);
			
			goto redo_again;
		}
		else{
			
			count ++; 
			head->curr_task= task;
			acquire_irq_lock(&head->curr_task->dialation_lock,flags);
			//head->curr_task->virt_start_time = lxc->linux_task->virt_start_time;

			/* This task cannot run now. need to look for another task */
			if(task->wakeup_time != 0 && task->wakeup_time > expected_time) 
			{

				release_irq_lock(&head->curr_task->dialation_lock,flags);
				if(schedule_list_size(lxc) == 1 || count == schedule_list_size(lxc)) 
				{
					/* this is the only task or all tasks are simultaneously asleep. we have have problem ? */
					PDEBUG_I("Get Next Valid Task:  ERROR : All tasks simultaneously asleep\n");
					return NULL; // for now.
				}

				/* requeue the task to the tail */
				requeue_schedule_list(lxc); 		
				head = schedule_list_get_head(lxc);
			}		
			else{
				release_irq_lock(&head->curr_task->dialation_lock,flags);
				return head;

			}
		}

	}while(head != NULL);

	/* Queue is empty. container stopped */
	lxc->stopped = -1;
	return NULL; 	

	
}


/***
Add process and recursively its children to the run queue of the lxc
***/
void add_process_to_schedule_queue_recurse(struct dilation_task_struct * lxc, struct task_struct *aTask, s64 window_duration, s64 expected_inc){

	struct list_head *list;
	struct task_struct *taskRecurse;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	ktime_t ktime;
	struct timeval ktv;
	s64 now;
	unsigned long flags;

	if (aTask == NULL) {
       	PDEBUG_I("Add Process To Schedule Queue: Task does not exist\n");
        return;
	}
	if (aTask->pid == 0) {
		PDEBUG_I("Add Process To Schedule Queue: pid is 0 cannot be added to schedule queue\n");
		return;
	}


	acquire_irq_lock(&aTask->dialation_lock,flags);
	aTask->dilation_factor = lxc->linux_task->dilation_factor;
	release_irq_lock(&aTask->dialation_lock,flags);
	me = aTask;
	t = me;
	do {
		acquire_irq_lock(&aTask->dialation_lock,flags);
		t->dilation_factor = aTask->dilation_factor;
		release_irq_lock(&aTask->dialation_lock,flags);
		t->static_prio = aTask->static_prio;
		add_to_schedule_list(lxc,t,FREEZE_QUANTUM,exp_highest_dilation);
	} while_each_thread(me, t);

	/* If task already exists, schedule queue would not be modified */
	add_to_schedule_list(lxc,aTask,FREEZE_QUANTUM,exp_highest_dilation); 
	list_for_each(list, &aTask->children)
    {
		taskRecurse = list_entry(list, struct task_struct, sibling);
		if (taskRecurse->pid == 0) {
				continue;
		}
		taskRecurse->static_prio = lxc->linux_task->static_prio; // *** trying
		add_process_to_schedule_queue_recurse(lxc,taskRecurse,FREEZE_QUANTUM,exp_highest_dilation);
	}



}

/***
Refresh the run queue of the lxc at the start of every round to add new processes
***/
void refresh_lxc_schedule_queue(struct dilation_task_struct *aTask,s64 window_duration, s64 expected_inc){

	if(aTask != NULL){
		add_process_to_schedule_queue_recurse(aTask,aTask->linux_task,FREEZE_QUANTUM,exp_highest_dilation);
	}
}




/***
Unfreeze process at head of schedule queue of container, run it with possible switches for the run time. Returns the time left in this round.
***/ 
int run_schedule_queue_single_core_mode(struct dilation_task_struct * lxc, lxc_schedule_elem * head, s64 remaining_run_time, s64 expected_time){

	struct list_head *list;
	struct task_struct * curr_task;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	ktime_t ktime;
	struct timeval ktv;
	s64 now;
	s64 timer_fire_time;
	s64 rem_time;
	s32 rem;
	s64 now_ns;
	s64 start_time;
	s64 last_run_freeze_time;
	struct poll_helper_struct * helper = NULL;
	struct select_helper_struct * select_helper = NULL;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
    unsigned long flags;
    int CPUID = lxc->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);

	if(head->duration_left <= 0 || remaining_run_time <= 0){
		PDEBUG_E("Run Schedule Queue Head Process: ERROR Cannot run task. duration left is 0");
		return remaining_run_time;

	}

	if(head->duration_left < remaining_run_time){
		timer_fire_time = head->duration_left;
		rem_time = remaining_run_time - head->duration_left;
	}
	else{
		timer_fire_time = remaining_run_time;
		rem_time = 0;
	}

	

	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);
	start_time = now_ns;
	curr_task = head->curr_task;
	me = curr_task;
	t = me;	
	
	if(lxc->last_run == NULL) {
	    last_run_freeze_time = start_time;
	}
	else if (find_task_by_pid(lxc->last_run->pid) != NULL) {
	    last_run_freeze_time = lxc->last_run->curr_task->freeze_time;
	}
	else{
	    last_run_freeze_time = start_time;
	}
	
	if(lxc->last_timer_fire_time == 0)
    	lxc->last_timer_fire_time = start_time;
    else
        lxc->last_timer_fire_time = last_run_freeze_time;
	
	acquire_irq_lock(&t->dialation_lock,flags);
	task_poll_helper = hmap_get_abs(&poll_process_lookup,t->pid);
	task_select_helper = hmap_get_abs(&select_process_lookup,t->pid);
	task_sleep_helper = hmap_get_abs(&sleep_process_lookup, t->pid);

	if(task_poll_helper != NULL){
		
		t->freeze_time = 0;
		atomic_set(&task_poll_helper->done,1);
		wake_up(&task_poll_helper->w_queue);
		release_irq_lock(&t->dialation_lock,flags);
	}
	else if(task_select_helper != NULL){

		t->freeze_time = 0;
		atomic_set(&task_select_helper->done,1);
		PDEBUG_V("Select Wakeup. Pid = %d\n", t->pid); 
		wake_up(&task_select_helper->w_queue);
		release_irq_lock(&t->dialation_lock,flags);
	}
	else if( task_sleep_helper != NULL) {
	
		/* Sending a Continue signal here will wake all threads up. We dont want that */
		t->freeze_time = 0;
		atomic_set(&task_sleep_helper->done,1);
		wake_up(&task_sleep_helper->w_queue);
		release_irq_lock(&t->dialation_lock,flags);
	}
	else if (t->freeze_time > 0)
   	{	
		t->freeze_time = 0;
		release_irq_lock(&t->dialation_lock,flags);
	    kill(t, SIGCONT, NULL);
    }
	else {
	    t->freeze_time = 0;
		release_irq_lock(&t->dialation_lock,flags);
       	PDEBUG_V("Run Schedule Queue Head Process: Thread not frozen pid: %d dilation %d\n", t->pid, t->dilation_factor);   	
		kill(t, SIGCONT, NULL);
	}
		

	do_gettimeofday(&now);
	now_ns = timeval_to_ns(&now);
	start_time = now_ns;
	    
    lxc->last_run = head; 
	lxc->last_timer_duration = timer_fire_time;
	ktime = ktime_set( 0, timer_fire_time );
	int ret;
	set_current_state(TASK_INTERRUPTIBLE);
	if(experiment_type != CS){
		hrtimer_start(&lxc->timer,ns_to_ktime(ktime_to_ns(ktime_get()) + timer_fire_time) ,HRTIMER_MODE_ABS);
		schedule();
	}
	else{
		hrtimer_start(&lxc->timer,ktime,HRTIMER_MODE_REL);
		wait_event_interruptible(lxc->tl->unfreeze_proc_queue,atomic_read(&lxc->tl->hrtimer_done) == 1);
		atomic_set(&lxc->tl->hrtimer_done,0);
	}
	set_current_state(TASK_RUNNING);
	curr_process_finished_flag[CPUID] = 0;

	head->duration_left = head->duration_left - timer_fire_time;
	if(head->duration_left <= 0){
		PDEBUG_V("Run Schedule Queue Head Process: Resetting head to duration of %lld\n", head->share_factor);
		head->duration_left = head->share_factor;
		requeue_schedule_list(lxc);
	}
	

	me = curr_task;
	t = me;
	do_gettimeofday(&now);		
	now_ns = timeval_to_ns(&now); 


	acquire_irq_lock(&t->dialation_lock,flags);
	t->freeze_time = lxc->last_timer_fire_time + lxc->last_timer_duration;
	release_irq_lock(&t->dialation_lock,flags);
	kill(t, SIGSTOP, NULL);
	/* set the last run task */	
	lxc->last_run = head;
	
		

	 
	return rem_time;

	
}



/***
Unfreezes and runs each process in shared timeslice mode
***/
int unfreeze_proc_exp_single_core_mode(struct dilation_task_struct *aTask, s64 expected_time) {
	struct timeval now;
	s64 now_ns;
	s64 start_ns;
	struct hrtimer * alt_timer = &aTask->timer;
	int CPUID = aTask->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
	int i = 0;
	unsigned long flags;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
	s64 virt_time;
	s64 change_vt;
	s64 rem_time;
	lxc_schedule_elem * head;
	s64 err;
	s32 rem;

    
	if (aTask->linux_task->freeze_time == 0)
	{
		PDEBUG_I("Unfreeze Proc Exp Recurse: Process not frozen pid: %d dilation %d in recurse\n", aTask->linux_task->pid, aTask->linux_task->dilation_factor);
		return -1;
	}
	
	atomic_set(&wake_up_signal_sync_drift[CPUID],0);



	/* for adding any new tasks that might have been spawned */
	refresh_lxc_schedule_queue(aTask,aTask->running_time,expected_time); 
	

	/* Set all past physical times */	
	do_gettimeofday(&now);
    now_ns = timeval_to_ns(&now);
    start_ns = now_ns;
	aTask->last_timer_fire_time = 0;
	
	
	
	/* TODO: Needed to handle this case separately for some wierd bug which causes a crash */
	if(schedule_list_size(aTask) == 1){

        lxc_schedule_elem * head;
        head = get_next_valid_task(aTask,expected_time);		
		PDEBUG_V("Unfreeze Proc Exp Recurse: Single process LXC on CPU %d\n",CPUID);
		acquire_irq_lock(&aTask->linux_task->dialation_lock,flags);
        if(aTask->linux_task->freeze_time > 0) { 
           
			aTask->linux_task->past_physical_time = aTask->linux_task->past_physical_time + (now_ns - aTask->linux_task->freeze_time);
			aTask->linux_task->freeze_time = 0;
			
			task_poll_helper = hmap_get_abs(&poll_process_lookup,aTask->linux_task->pid);
			task_select_helper = hmap_get_abs(&select_process_lookup,aTask->linux_task->pid);
			task_sleep_helper = hmap_get_abs(&sleep_process_lookup,aTask->linux_task->pid);
			
			if(task_poll_helper == NULL && task_select_helper == NULL && task_sleep_helper == NULL){			
				release_irq_lock(&aTask->linux_task->dialation_lock,flags);
				kill(aTask->linux_task, SIGCONT, NULL);
            }
            else {
				if(task_poll_helper != NULL){				
					atomic_set(&task_poll_helper->done,1);
					wake_up(&task_poll_helper->w_queue);
					release_irq_lock(&aTask->linux_task->dialation_lock,flags);
				}
				else if(task_select_helper != NULL){					
					atomic_set(&task_select_helper->done,1);
					PDEBUG_V("Select Wakeup. Pid = %d\n", aTask->linux_task->pid); 
					wake_up(&task_select_helper->w_queue);
					release_irq_lock(&aTask->linux_task->dialation_lock,flags);	
				}
				else if( task_sleep_helper != NULL) {				
					atomic_set(&task_sleep_helper->done,1);		
					wake_up(&task_sleep_helper->w_queue);
					release_irq_lock(&aTask->linux_task->dialation_lock,flags);
				}
				else {
					release_irq_lock(&aTask->linux_task->dialation_lock,flags);
					kill(aTask->linux_task,SIGCONT,NULL);
				}
 			}
        }
        else{
            release_irq_lock(&aTask->linux_task->dialation_lock,flags);
            kill(aTask->linux_task,SIGCONT,NULL);
        }
  		
		ktime_t ktime;
		ktime = ktime_set(0,aTask->running_time);
		
		
		aTask->last_timer_fire_time = start_ns;
		aTask->last_timer_duration = aTask->running_time;
		set_current_state(TASK_INTERRUPTIBLE);
		
		if(experiment_type != CS){
			hrtimer_start(&aTask->timer,ns_to_ktime(ktime_to_ns(ktime_get()) + aTask->running_time) ,HRTIMER_MODE_ABS);
			schedule();
		}
		else{
			hrtimer_start(&aTask->timer,ktime,HRTIMER_MODE_REL);
			wait_event_interruptible(aTask->tl->unfreeze_proc_queue,atomic_read(&aTask->tl->hrtimer_done) == 1);
			atomic_set(&aTask->tl->hrtimer_done,0);
		}
		set_current_state(TASK_RUNNING);
		do_gettimeofday(&now);
    	now_ns = timeval_to_ns(&now);

		PDEBUG_V("Unfreeze Proc Exp Recurse: Single process on CPU %d resumed\n",CPUID);

		mutex_lock(&exp_mutex);

		if(aTask->linux_task->dilation_factor == 0){
			err = now_ns - start_ns - aTask->running_time;
		}
		else if(aTask->linux_task->dilation_factor > 0){
			err = div_s64_rem((now_ns - start_ns - aTask->running_time)*1000 ,aTask->linux_task->dilation_factor,&rem);
		}
		else{
			err = 0;
		}

		if(err >= 0){
		round_error = round_error + err;
		round_error_sq = round_error_sq + (err*err);
		n_rounds = n_rounds + 1;
		}
		mutex_unlock(&exp_mutex);
		
		aTask->last_run = head;		
		kill(aTask->linux_task, SIGSTOP, NULL);
		acquire_irq_lock(&aTask->linux_task->dialation_lock,flags);	
        aTask->linux_task->freeze_time = start_ns + aTask->running_time;
        release_irq_lock(&aTask->linux_task->dialation_lock,flags);      
	
		freeze_proc_exp_recurse(aTask);	
	}
	else {
	
	rem_time = aTask->running_time;
	set_all_past_physical_times_recurse(aTask->linux_task, start_ns,0,aTask);
	do{

		PDEBUG_V("Unfreeze Proc Exp Recurse: Getting next valid task on CPU : %d for lxc : %d\n",CPUID, aTask->linux_task->pid);
		head = get_next_valid_task(aTask,expected_time);
		if(head == NULL){
			/* need to stop container here */
			PDEBUG_I("Unfreeze Proc Exp Recurse: ERROR. Need to stop container\n");
			return 0;
		}		
		
		
		
		atomic_set(&wake_up_signal_sync_drift[CPUID],0);
		PDEBUG_V("TimeKeeper : Unfreeze Proc Exp Recurse: Running next valid task on CPU : %d for lxc : %d\n",CPUID, aTask->linux_task->pid);
		rem_time  = run_schedule_queue_single_core_mode(aTask, head, rem_time, expected_time);
		set_all_freeze_times_recurse(aTask->linux_task,start_ns + aTask->running_time - rem_time,aTask->running_time,0);
		i++;	
	}while(rem_time > 0 && schedule_list_size(aTask) > 1);

	/* Set all freeze times */
	set_all_freeze_times_recurse(aTask->linux_task,start_ns + aTask->running_time - rem_time,aTask->running_time,0);
	
	}
	
	do_gettimeofday(&now);
	virt_time = get_virtual_time(aTask,timeval_to_ns(&now) );
	if(virt_time > expected_time) {
	    change_vt = virt_time - expected_time;
	    if(change_vt > 0) {
		    PDEBUG_I("Here in the future: Pid = %d, Virtual time Error = %llu. \n",aTask->linux_task->pid,change_vt);
		}
	
	}
	else{
	    change_vt = expected_time - virt_time;
		if(change_vt > 0) {
			PDEBUG_I("Here in the past: Pid = %d, rem_time = %llu, Virtual time Error = %llu\n",aTask->linux_task->pid, rem_time, change_vt);
			aTask->running_time = change_vt;
    		return unfreeze_proc_exp_single_core_mode(aTask,expected_time);
		}
	}
    return 0;
}


/***
Unfreeze process at head of schedule queue of container, run it with possible switches for the run time. Returns the time left in this round.
***/ 
int run_schedule_queue_multi_core_mode(struct dilation_task_struct * lxc, lxc_schedule_elem * head, s64 start_time, s64 vt_advance){

	struct list_head *list;
	struct task_struct * curr_task;
	struct dilation_task_struct *dilTask;
	struct task_struct *me;
	struct task_struct *t;
	ktime_t ktime;
	struct timeval ktv;
	s64 now;
	s64 timer_fire_time;
	s64 rem_time;
	s32 rem;
	s64 now_ns;
	s64 last_run_freeze_time;
	unsigned int sleep_duration = 0;
	struct poll_helper_struct * helper = NULL;
	struct select_helper_struct * select_helper = NULL;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
    unsigned long flags;
    int CPUID = lxc->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
    
    curr_task = head->curr_task;
	t = curr_task;	
	timer_fire_time = vt_advance;
	lxc->last_timer_fire_time = start_time;
	

	acquire_irq_lock(&t->dialation_lock,flags);
	task_poll_helper = hmap_get_abs(&poll_process_lookup,t->pid);
	task_select_helper = hmap_get_abs(&select_process_lookup,t->pid);
	task_sleep_helper = hmap_get_abs(&sleep_process_lookup,t->pid);

	
	if(task_poll_helper != NULL){
		atomic_set(&task_poll_helper->done,1);
		release_irq_lock(&t->dialation_lock,flags);		
		wake_up(&task_poll_helper->w_queue);
		
	}
	else if(task_select_helper != NULL){
		atomic_set(&task_select_helper->done,1);
		release_irq_lock(&t->dialation_lock,flags);
		wake_up(&task_select_helper->w_queue);
		
	}
	else if( task_sleep_helper != NULL) {
	
		/* Sending a Continue signal here will wake all threads up. We dont want that */
		atomic_set(&task_sleep_helper->done,1);
		release_irq_lock(&t->dialation_lock,flags);
		wake_up(&task_sleep_helper->w_queue);
	}
	else 
   	{	
		release_irq_lock(&t->dialation_lock,flags);
	    kill(t, SIGCONT, NULL);

    }
		
    lxc->last_run = head; 
	lxc->last_timer_duration = timer_fire_time;
	ktime = ktime_set( 0, timer_fire_time );
	int ret;

	set_current_state(TASK_INTERRUPTIBLE);	
	if(experiment_type != CS){
		hrtimer_start(&lxc->timer,ns_to_ktime(ktime_to_ns(ktime_get()) + timer_fire_time) ,HRTIMER_MODE_ABS);
		schedule();
	}
	else{
		hrtimer_start(&lxc->timer,ktime,HRTIMER_MODE_REL);
		wait_event_interruptible(lxc->tl->unfreeze_proc_queue,atomic_read(&lxc->tl->hrtimer_done) == 1);
		atomic_set(&lxc->tl->hrtimer_done,0);
	}
	
	curr_process_finished_flag[CPUID] = 0;
	set_current_state(TASK_RUNNING);
		
	requeue_schedule_list(lxc);
	t = curr_task;
	
    if(find_task_by_pid(head->pid) != NULL) {
	    kill(t, SIGSTOP, NULL);
	}
	
	return 0;

	
}



/***
Unfreezes the container, then calls unfreeze_children to unfreeze all of the children
***/
int unfreeze_proc_exp_multi_core_mode(struct dilation_task_struct *aTask, s64 expected_time) {
	struct timeval now;
	s64 now_ns;
	s64 start_ns;
	struct hrtimer * alt_timer = &aTask->timer;
	int CPUID = aTask->cpu_assignment - (TOTAL_CPUS - EXP_CPUS);
	int i = 0;
	unsigned long flags;
	struct poll_helper_struct * task_poll_helper = NULL;
	struct select_helper_struct * task_select_helper = NULL;
	struct sleep_helper_struct * task_sleep_helper = NULL;
	s64 virt_time;
	s64 change_vt;
	s64 rem_time;
	unsigned int sleep_duration;
	s32 rem;

    aTask->last_timer_fire_time = 0;
    
	if (aTask->linux_task->freeze_time == 0)
	{
		return -1;
	}
        
	atomic_set(&wake_up_signal_sync_drift[CPUID],0);

	
	/* for adding any new tasks that might have been spawned */
	refresh_lxc_schedule_queue(aTask,aTask->running_time,expected_time); 
		
    do_gettimeofday(&now);
    now_ns = timeval_to_ns(&now);
    start_ns = now_ns;
    aTask->wake_up_time = start_ns;
    
    /* Set all past physical times */
    lxc_schedule_elem * head;

    do{

	    head = get_next_valid_task(aTask,expected_time);
	    if(head == NULL){
		    return 0;
	    }
	
	    atomic_set(&wake_up_signal_sync_drift[CPUID],0);
	    run_schedule_queue_multi_core_mode(aTask, head, start_ns,aTask->running_time);
	    i++;
    }while(i < schedule_list_size(aTask));

    
	
    return 0;
}


/***
Unfreezes the container, then calls unfreeze_children to unfreeze all of the children
***/
int unfreeze_proc_exp_recurse(struct dilation_task_struct *aTask, s64 expected_time) {

	if(experiment_type != CS) {
		#ifdef MULTI_CORE_NODES
			return unfreeze_proc_exp_multi_core_mode(aTask,expected_time);
		#else
			return unfreeze_proc_exp_single_core_mode(aTask,expected_time);
		#endif
	}
	else{
		return unfreeze_proc_exp_single_core_mode(aTask,expected_time);
	}

}

