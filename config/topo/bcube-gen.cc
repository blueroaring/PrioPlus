/*
 * This is a bcube topology generator.
 * To compile it, run `g++ bcube-gen.cc -o bcube-gen`
 * Until now, this generator only supports single link type, i.e., 0.
 */

#include <assert.h>
#include <fstream>
#include <iostream>
#include <math.h>

using namespace std;

//==============global setting=================//
int n = 4; // number of servers in a pod, the number of ports in a switch
int k = 2; // dimension of bcube

int
main()
{
    cout << "k for bcube: ";
    cout.flush();
    cin >> k;

    cout << "n for bcube: ";
    cout.flush();
    cin >> n;

    string file;
    cout << "The output file name: ";
    cout.flush();
    cin >> file;
    ofstream fout(file.c_str());

    int host_num = pow(n, k + 1);
    int sw_num = 0;
    for (int i = 0; i <= k; i++)
    {
        if (i < k)
        {
            sw_num += (pow(n, i) * n);
        }
        else
        {
            sw_num += pow(n, i);
        }
    }
    int link_num = n * sw_num;
    fout << host_num + sw_num << " " << sw_num << " " << link_num << endl;

    for (int i = host_num; i < host_num + sw_num; i++)
    {
        fout << i << " ";
    }
    fout << endl;

    int sw_start = host_num; // the start idx of switches in each level
    int pre_pod_sw = 0;      // the number of switches in pre levelf
    for (int l = 0; l <= k; l++)
    {                              // connect switches and host in each level
        int pod_h = pow(n, l + 1); // the number of host in each pod in level l
        int pod_sw = pow(n, l);    // the number of switches in each pod in level l
        cout << "pod_h= " << pod_h << " pod_sw= " << pod_sw << endl;
        for (int i = 0; i < host_num; i++)
        { // connect all host to switches in level l
            if (i == 0)
            { // this is a new level, calculate the start idx of switches in this new level
                sw_start += pre_pod_sw;
                pre_pod_sw = pod_sw;
            }
            if (i % pod_h == 0 && i != 0)
            { // this is a new pod, calculate the start idx of switches in this new pod
                sw_start += pod_sw;
            }
            fout << i << " " << sw_start + (i) % (pod_sw) << " " << 0 << endl;
        }
    }
}
