import sys
import argparse
import os
import json


def main(argv):
    num_host_per_tor = 2
    num_tor_per_pod = 2
    num_agg_per_pod = 2
    num_pod = 1  #  for spine leaf topology, we just gen a pod in fat tree
        
    output_name = ""

    help_msg = "fat-tree-gen.py -s server -t tor -a agg -p pod -o output_file_name \n" + \
               "eg: python3 fat-tree-gen.py -s 2 -t 2 -a 2 -p 2 -o sl222.txt"

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="number of hosts", type=int)
    parser.add_argument("-t", "--tor", help="number of tors", type=int)
    parser.add_argument("-a", "--agg", help="number of aggs", type=int)
    parser.add_argument("-o", "--output", help="output file name")
    args = parser.parse_args()

    if not args.server or not args.tor or not args.agg:
        print(help_msg)
        sys.exit(1)
    if args.server:
        num_host_per_tor = args.server
    if args.tor:
        num_tor_per_pod = args.tor
    if args.agg:
        num_agg_per_pod = args.agg
    if args.output:
        output_name = args.output

    # nodes include hosts and switches
    num_host = num_host_per_tor * num_tor_per_pod * num_pod
    num_switch_per_pod = num_tor_per_pod + num_agg_per_pod
    num_tor = num_tor_per_pod * num_pod
    num_node = (num_host_per_tor * num_tor_per_pod + num_tor_per_pod + num_agg_per_pod) * num_pod
    num_link = 0

    # generate the topo into a string, where each line is a link in format "src dst type"
    topo = ""
    topo_host = ""
    topo_intrapod = ""
    topo_interpod = ""
    switch_begin = num_host
    
    # generate the links between host and tor with type host
    i = 0 # only one pod
    for j in range(num_tor_per_pod):
        tor_idx = i * num_tor_per_pod + j + switch_begin
        for k in range(num_host_per_tor):
            host_idx = i * num_host_per_tor * num_tor_per_pod + j * num_host_per_tor + k
            topo_host += str(host_idx) + " " + str(tor_idx) + " host\n"
            num_link += 1
    
    # generate the links between tor and agg with type switch
    for j in range(num_tor_per_pod):
        tor_idx = i * num_tor_per_pod + j + switch_begin
        for k in range(num_agg_per_pod):
            agg_idx = i * num_agg_per_pod + k + switch_begin + num_tor
            topo_intrapod += str(tor_idx) + " " + str(agg_idx) + " switch\n"
            num_link += 1
    
    topo = topo_host + topo_intrapod + topo_interpod

    # open the file
    def open_file(filename):
        try:
            return open(filename, "w")
        except:
            print("Error: cannot open file " + filename)
            sys.exit(1)

    # close the file
    def close_file(file):
        file.close()

    file = open_file(output_name)

    # write the header: num_node num_switch num_links
    file.write(str(num_node) + " " + str(num_node -
               num_host) + " " + str(num_link) + "\n")
    # write the second line: id of switches
    for i in range(num_host, num_node):
        file.write(str(i) + " ")
    file.write("\n")
    # write the topo
    file.write(topo)

    close_file(file)
    
    # print a list of host
    for i in range(num_host_per_tor):
        for j in range(num_tor_per_pod):
            print(i + j * num_host_per_tor, end=",")


if __name__ == "__main__":
    main(sys.argv[1:])
