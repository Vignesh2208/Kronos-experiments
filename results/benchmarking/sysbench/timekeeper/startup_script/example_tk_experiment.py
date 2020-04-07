import sys
import os
import time
import timekeeper_functions as tf
import sys
import argparse


def start_new_dilated_process(cmd_to_run, log_file_fd, tdf):
    newpid = os.fork()
    if newpid == 0:
        os.dup2(log_file_fd, sys.stdout.fileno())
        os.dup2(log_file_fd, sys.stderr.fileno())
        args = cmd_to_run.split(' ')
        #time.sleep(1.0)
        print "Running: " , args
        os.execvp(args[0], args)
    else:
        print "Dilating and Adding to Experiment: ", newpid
        tf.dilate_all(newpid, tdf)
        tf.addToExp(newpid)
        return newpid



def main():

    
    parser = argparse.ArgumentParser()

    parser.add_argument('--cmds_to_run_file', dest='cmds_to_run_file',
                        help='path to file containing commands to run', \
                        type=str, default='cmds_to_run_file.txt')


    parser.add_argument('--tdf', dest='tdf',
                        help='time dilation factor', type=float, \
                        default=1.0)

    parser.add_argument('--timeslice', dest='timeslice',
                        help='Timeslice (ns)', type=int,
                        default=1000)

    parser.add_argument('--num_progress_rounds', dest='num_progress_rounds',
                        help='Number of rounds to run', type=int,
                        default=2000000)
    parser.add_argument('--num_times', dest='num_times',
                        help='Number of times to re-run experiment', type=int,
                        default=5)
    parser.add_argument('--output_dir', dest='output_dir', type=str, default='/tmp')

    args = parser.parse_args()

    if args.tdf < 0.0:
       args.tdf = 1.0
    args = parser.parse_args()

    count = 1
    while count <= args.num_times:

        log_fds = []
        cmds_to_run = []
        pids = []
        os.system('sudo rm /tmp/trigger.txt')
        os.system('sudo rm /tmp/tracer*')

    
        if not os.path.isfile(args.cmds_to_run_file):
	    print "Commands file path is incorrect !"
	    sys.exit(0)
        fd1 = open(args.cmds_to_run_file, "r")
        cmds_to_run = [x.strip() for x in fd1.readlines()]
        fd1.close()
        for i in xrange(0, len(cmds_to_run)) :
	    with open("/tmp/tracer_log%d.txt" %(i), "w") as f:
	        pass
        log_fds = [ os.open("/tmp/tracer_log%d.txt" %(i), os.O_RDWR | os.O_CREAT ) \
	    for i in xrange(0, len(cmds_to_run)) ]

        print "Removing existing TimeKeeper module"     
        os.system('rmmod /home/kronos/TimeKeeper/dilation-code/build/TimeKeeper.ko')
        print "Inserting TimeKeeper module"
        time.sleep(1.0)
        os.system('insmod /home/kronos/TimeKeeper/dilation-code/build/TimeKeeper.ko')

    
        print "Starting all commands to run !"
    
        for i in xrange(0, len(cmds_to_run)):
	    print "Starting cmd: %s" %(cmds_to_run[i])
	    pid = start_new_dilated_process(cmds_to_run[i], log_fds[i], args.tdf)
	    pids.append(pid)
    
        print "Setting Experiment timeslice ..."
        tf.set_cbe_experiment_timeslice(args.timeslice*args.tdf)
        print "Synchronizing and freezing ..."
        tf.synchronizeAndFreeze()
        os.system('sudo touch /tmp/trigger.txt') 


        print "Starting Synchronized Experiment !"
        tf.startExp()
        start_time = float(time.time())
        if args.num_progress_rounds > 0 :
	    print "Running for %d rounds * ... " %(args.num_progress_rounds)

	    num_finised_rounds = 0
	    step_size = args.num_progress_rounds
	    while num_finised_rounds < args.num_progress_rounds:
	    	#print "Num finished rounds: ", num_finised_rounds
	        tf.progress_exp_cbe(step_size)
	        num_finised_rounds += step_size
            
			#if num_finised_rounds == args.num_progress_rounds/2:
                #    ret = raw_input('Half way through')
	        #print "Ran %d rounds ..." %(num_finised_rounds)

        elapsed_time = float(time.time()) - start_time
        total_elapsed_virtual_time = float(args.num_progress_rounds * args.timeslice/args.tdf)/1000000000.0
        #raw_input("Press Enter to continue...")
        tf.resume_exp_cbe()
        with open('/tmp/exp_stats.txt', 'w') as f:
            f.write('overhead ratio: %f\n' %(elapsed_time/total_elapsed_virtual_time))
            f.write('total_elapsed_virtual_time: %f\n' %(total_elapsed_virtual_time))
            f.write('elapsed_time: %f\n' %(elapsed_time))
        print "Stopping Synchronized Experiment !"
        tf.stopExp()
        print "Waiting 10 secs ..."
        time.sleep(1.0)
        for fd in log_fds:
	    os.close(fd)
	    #for pid in pids:
	    os.system('sudo killall sysbench')

        os.system('mkdir -p %s/run_%d' %(args.output_dir, count))
        os.system('cp /tmp/tracer* %s/run_%d' %(args.output_dir, count))
        os.system('cp /tmp/exp_stats.txt %s/run_%d' %(args.output_dir, count))
        print "Finished ! Logs of each ith tracer can be found in %s/run_%d" %(args.output_dir, count)
        count += 1
        
            

if __name__ == "__main__":
	main()
