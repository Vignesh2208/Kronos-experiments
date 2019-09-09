import plotly.graph_objects as go
import numpy as np
import sys
import math

folder_name = 'campus_http{type}_{numLXCs}LXCs_200_hosts_per_switch_20secs'


hop_counts = {
	'1-1': 0,
	'1-2': 4,
        '1-3': 3,
        '1-4': 7,
        '1-5': 8,
        '1-6': 8,
        '1-7': 8,
	'1-8': 6,
	'1-9': 6,
	'1-10': 7,
	'1-11': 7,
	'1-12': 7,
	'2-2': 0,
	'2-3': 7,
	'2-4': 3,
	'2-5': 4,
	'2-6': 4,
	'2-7': 4,
	'2-8': 6,
	'2-9': 6,
	'2-10': 7,
	'2-11': 7,
	'2-12': 7
}

def get_hop_count (src_lxc_no, dst_lxc_no, num_lxcs_per_lan):
	assert src_lxc_no == 1 or src_lxc_no == 2
	if num_lxcs_per_lan == 1:
		return hop_counts['%d-%d'%(src_lxc_no, dst_lxc_no)]
	src_lan_no = int(src_lxc_no /num_lxcs_per_lan)
	dst_lan_no = int(dst_lxc_no /num_lxcs_per_lan)

	if src_lxc_no % num_lxcs_per_lan != 0:
		src_lan_no += 1
	if dst_lxc_no % num_lxcs_per_lan != 0:
		dst_lan_no += 1
	
	return hop_counts['%d-%d'%(src_lan_no, dst_lan_no)] + 2


num_lxcs_per_lan = [1, 5, 10]
http_types = ['2', '11']

hop_values = {}

for num_lxcs in num_lxcs_per_lan:
	for http_type in http_types:
		for dst_lxc in range(3, num_lxcs*12 + 1):
			content = []
			with open(folder_name.format(type=http_type, numLXCs=num_lxcs) + ('/lxc-%d-cmds.txt' %(dst_lxc)), 'r') as f:
				content = f.readlines()
			server_ip = content[0].split(' ')[1]
			assert server_ip == '10.1.1.1' or server_ip == '10.1.1.2'
			src_lxc_no = 1 if server_ip == '10.1.1.1' else 2
			n_hops = get_hop_count(src_lxc_no, dst_lxc, num_lxcs)
			if http_type not in hop_values:
				hop_values[http_type] = {}
				hop_values[http_type][num_lxcs] = {}
			if num_lxcs not in hop_values[http_type]:
				hop_values[http_type][num_lxcs] = {}

			if n_hops not in hop_values[http_type][num_lxcs]:
				hop_values[http_type][num_lxcs][n_hops] = []
			with open(folder_name.format(type=http_type, numLXCs=num_lxcs) + ('/lxc-%d/lxc-%d.log' %(dst_lxc, dst_lxc)), 'r') as f:
				content = f.readlines()
			skip_next = False
			for line in content:
				if 'No route' in line:
					skip_next = True
				if 'Time elapsed for request completion:' in line:
					if skip_next:
						skip_next = False
						continue
					else:
						line_mod = line.rstrip()
						elapsed_time = float(line_mod.split(' ')[-1])
						hop_values[http_type][num_lxcs][n_hops].append(elapsed_time)

hop_counts = [2, 5, 6,8,9,10]
num_lxcs = [5, 10]
http2_mu = {}
http2_err = {}

http11_mu = {}
http11_err = {}

#print (hop_values)

for n_hops in hop_counts:
	for n_lxc in num_lxcs:
		if n_lxc not in http2_mu:
			http2_mu[n_lxc] = []
			http2_err[n_lxc] = []
			http11_mu[n_lxc] = []
			http11_err[n_lxc] = []
		http2_mu[n_lxc].append(np.mean(hop_values['2'][n_lxc][n_hops]))
		http2_err[n_lxc].append(1.96*np.std(hop_values['2'][n_lxc][n_hops])/math.sqrt(len(hop_values['2'][n_lxc][n_hops])))
		http11_mu[n_lxc].append(np.mean(hop_values['11'][n_lxc][n_hops]))
		http11_err[n_lxc].append(1.96*np.std(hop_values['11'][n_lxc][n_hops])/math.sqrt(len(hop_values['2'][n_lxc][n_hops])))

print ("Http2 Mean, Std: ", http2_mu, http2_err)
print ("Http11 Mean, Std: ", http11_mu, http11_err)

fig = go.Figure()
fig.add_trace(go.Scatter(
    x=hop_counts, y=http2_mu[5],
    error_y=dict(type='data', array=http2_err[5], thickness=1.5, width=3),
    name='HTTP2 60 Emulated, 2400 Simulated Hosts',
    marker=dict(color='blue', size=15, symbol='circle'),
    mode='markers',
    line=dict(
                color="blue",
                width=4.0,
                dash="dashdot",
            )
))
fig.add_trace(go.Scatter(
    x=hop_counts, y=http2_mu[10],
    error_y=dict(type='data', array=http2_err[10], thickness=1.5, width=3),
    name='HTTP2 120 Emulated, 2400 Simulated Hosts',
    marker=dict(color='blue', size=15, symbol="square" ),
    mode='markers',
    line=dict(
                color="blue",
                width=4.0,
                dash="dash",
            )
))
fig.add_trace(go.Scatter(
    x=hop_counts, y=http11_mu[5],
    error_y=dict(type='data', array=http11_err[5], thickness=1.5, width=3),
    name='HTTP11 60 Emulated, 2400 Simulated Hosts',
    marker=dict(color='green', size=15, symbol="triangle-down-dot"),
    mode='markers',
        line=dict(
                color="blue",
                width=4.0,
                dash="longdash",
            )
))

fig.add_trace(go.Scatter(
    x=hop_counts, y=http11_mu[10],
    error_y=dict(type='data', array=http11_err[10], thickness=1.5, width=3),
    name='HTTP11 120 Emulated, 2400 Simulated Hosts',
    marker=dict(color='green', size=15, symbol='triangle-up-dot'),
    mode='markers',
            line=dict(
                color="green",
                width=2.0,
                dash="solid",
            )
))


fig.update_layout(
    title=go.layout.Title(
        text="HTTP2 vs HTTP11 Request Completion Times",
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
            text="Request Completion Times (sec)",
            font=dict(
                family="Courier",
                size=25
            )
        )
    ),

    legend=go.layout.Legend(
	x = 0,
	y = 1,
        font=dict(
            family="sans-serif",
            size=20,
            color="black"
        ),
        bgcolor="White",
        bordercolor="Black",
        borderwidth=5
    )
    
)
#fig.update_layout(xaxis_type="log")

fig.update_xaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20))
fig.update_yaxes(title_font=dict(size=20, family='Courier'), tickfont=dict(family='Courier', size=20), nticks=4)
fig.update_xaxes(ticks="outside", tickwidth=5, ticklen=10)
fig.update_yaxes(ticks="outside", tickwidth=5, ticklen=10)

fig.write_image("http2_vs_http1.jpg", width=900, height=600)
			
