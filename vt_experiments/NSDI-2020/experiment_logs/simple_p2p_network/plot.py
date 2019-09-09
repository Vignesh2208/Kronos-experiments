import plotly.graph_objects as go
import numpy as np
import sys
import math

folder_name = 'p2p_grpc_1000_emu_hosts_{type}'

num_lxcs_per_lan = 10
num_server_lxcs_per_lan = 1
def get_hop_count (src_lxc_no, dst_lxc_no, num_lxcs_per_lan):
	src_lan_no = int(src_lxc_no /num_lxcs_per_lan)
	dst_lan_no = int(dst_lxc_no /num_lxcs_per_lan)

	if src_lxc_no % num_lxcs_per_lan != 0:
		src_lan_no += 1
	if dst_lxc_no % num_lxcs_per_lan != 0:
		dst_lan_no += 1
	
	return abs(dst_lan_no - src_lan_no) + 4

hop_values = {
	'kronos': {},
	'timekeeper': {}
}
nSwitches = 10
nClientLXCsperSwitch = 9
nServerLXCsperSwitch=1
nLXCsperSwitch=10

for i in range(1, nSwitches + 1):
	k = 1
	for j in range(1, nClientLXCsperSwitch + 1):
		if k == i:
			k = k + 1
		server_lxc_no = ((k-1)*nLXCsperSwitch + 1)
		client_lxc_no = (i - 1)*nLXCsperSwitch + j + nServerLXCsperSwitch
		hop_count = get_hop_count(client_lxc_no, server_lxc_no, num_lxcs_per_lan)
		if hop_count not in hop_values['kronos']:
			hop_values['kronos'][hop_count] = []
			hop_values['timekeeper'][hop_count] = []
		content = []
		with open(folder_name.format(type='kronos') + ('/lxc-%d/lxc-%d.log' %(client_lxc_no, client_lxc_no)), 'r') as f:
			content = f.readlines()
		skip_next = False
		for line in content:
			if 'Connect Failed' in line:
				skip_next = True
			if 'Time elapsed for request completion:' in line:
				if skip_next == True:
					skip_next = False
					continue
				else:
					line_mod = line.rstrip()
					elapsed_time = float(line_mod.split(' ')[5])
					hop_values['kronos'][hop_count].append(elapsed_time)

		content = []
		with open(folder_name.format(type='timekeeper') + ('/lxc-%d/lxc-%d.log' %(client_lxc_no,client_lxc_no)), 'r') as f:
			content = f.readlines()
		skip_next = False
		for line in content:
			if 'Connect Failed' in line:
				skip_next = True
			if 'Time elapsed for request completion:' in line:
				if skip_next == True:
					skip_next = False
					continue
				else:
					line_mod = line.rstrip()
					elapsed_time = float(line_mod.split(' ')[5])
					hop_values['timekeeper'][hop_count].append(elapsed_time)

	
	k = k + 1

hop_counts = sorted(hop_values['kronos'].keys())


kronos_mu = []
kronos_err = []

tk_mu = []
tk_err = []


for hop_count in hop_counts:
	kronos_mu.append(np.mean(hop_values['kronos'][hop_count]))
	kronos_err.append(1.96*np.std(hop_values['kronos'][hop_count])/math.sqrt(len(hop_values['kronos'][hop_count])))

	tk_mu.append(np.mean(hop_values['timekeeper'][hop_count]))
	tk_err.append(1.96*np.std(hop_values['timekeeper'][hop_count])/math.sqrt(len(hop_values['timekeeper'][hop_count])))


print ("Kronos Mean, Std: ", kronos_mu, kronos_err)
print ("TimeKeeper Mean, Std: ", tk_mu, tk_err)

measured = [0.0031, 0.00308, 0.00307, 0.00307, 0.00308, 0.00306 , 0.00311, 0.00312, 0.00308]
fig = go.Figure()
fig.add_trace(go.Scatter(
    x=hop_counts, y=kronos_mu,
    error_y=dict(type='data', array=kronos_err, thickness=1.5, width=3),
    name='Kronos',
    marker=dict(color='blue', size=15, symbol='circle'),
    mode='markers+lines',
    line=dict(
                color="blue",
                width=4.0,
                dash="dashdot",
            )
))
fig.add_trace(go.Scatter(
    x=hop_counts, y=tk_mu,
    error_y=dict(type='data', array=tk_err, thickness=1.5, width=3),
    name='TimeKeeper',
    marker=dict(color='green', size=15, symbol="square" ),
    mode='markers+lines',
    line=dict(
                color="green",
                width=4.0,
                dash="dash",
            )
))

fig.add_trace(go.Scatter(
    x=hop_counts, y=measured,
    name='Measured 1 Server 9 Clients',
    mode='lines',
    line=dict(
                color="red",
                width=4.0,
                dash="longdash",
            )
))


fig.update_layout(
    title=go.layout.Title(
        text="RPC Request Completion Times (1000 Emulated Hosts)",
	font=dict(
                family="Courier",
                size=30
            ),
	xanchor = "auto",
	yanchor = "middle",
	x=0
    ),
    xaxis=go.layout.XAxis(
        title=go.layout.xaxis.Title(
            text="Hop distance from server",
            font=dict(
                family="Courier",
                size=25
            )
        ),
    ),
    yaxis=go.layout.YAxis(
        title=go.layout.yaxis.Title(
            text="RPC Completion Times (sec)",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),

    legend=go.layout.Legend(
	x=-.05,
	y=1.1,
        font=dict(
            family="sans-serif",
            size=20,
            color="black"
        ),
        #bgcolor="White",
        #bordercolor="Black",
        #borderwidth=5
    )
    
)
fig.update_layout(yaxis_type="log")
fig.update_layout(legend_orientation="h")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=6, dtick=1)
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), dtick=1)
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

fig.write_image("rpc_comparison.jpg", width=900, height=600)

			
