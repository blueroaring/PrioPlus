import sys, argparse, os, json

def main(argv):
    num_server = 0
    output_name = ""

    help_msg = "training-topo-gen.py -s num_server -o output_file_name \n" + \
               "eg: python3 training-topo-gen.py -s 4 -o star4.txt"

    parser = argparse.ArgumentParser()
    parser.add_argument("-s", "--server", help="number of servers", type=int)
    parser.add_argument("-o", "--output", help="output file name")
    args = parser.parse_args()

    if args.server:
        num_server = args.server
    if args.output:
        output_name = args.output

    # num_node = num_server + 1 for a swith in the star topo
    num_node = num_server + 1

    # generate the topo into a string, where each line is a link in format "src dst type"
    topo = ""
    topo += str(num_node) + " " + str(1) +  " " + str(num_server) + "\n"
    topo += str(num_node-1) + "\n"  # id of the switch
    
    # generate the links between switch and servers with type "0"
    for i in range(num_server):
        topo += str(i) + " " + str(num_server) + " 0\n"

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
    
    # write the topo
    file.write(topo)

    close_file(file)


if __name__ == "__main__":
    main(sys.argv[1:])
