#!/bin/bash

export LC_NUMERIC="en_US.UTF-8"

parameters=( "$@" )
	
conProb=$(echo ${parameters[0]})

for i in 1 2 3 4 5 6 7 8 9 10;
do
outFile=tree_res_${conProb}_${i}.ned



echo "package networks;

network tree_res_network extends base_network{

    parameters:
        //Number of ccn nodes
    	n = 15;

	//Number of repositories
	node_repos = \"0\";
	num_repos = 1;
	replicas = 1;

        //Number of clients
	num_clients = 8;
	node_clients = \"7,8,9,10,11,12,13,14\";



connections allowunconnected:

// Tree core " >> ${outFile}


echo "node[0].face++ <--> { delay = 1ms; } <-->node[1].face++;
node[0].face++ <--> { delay = 1ms; } <-->node[2].face++;
node[1].face++ <--> { delay = 1ms; } <-->node[3].face++;
node[1].face++ <--> { delay = 1ms; } <-->node[4].face++;" >> ${outFile}

## 1 --> 5
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[1].face++ <--> { delay = 1ms; } <-->node[5].face++;" >> ${outFile}
fi
## 1 --> 6
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[1].face++ <--> { delay = 1ms; } <-->node[6].face++;" >> ${outFile}
fi
## 2 --> 3
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[2].face++ <--> { delay = 1ms; } <-->node[3].face++;" >> ${outFile}
fi
## 2 --> 4
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[2].face++ <--> { delay = 1ms; } <-->node[4].face++;" >> ${outFile}
fi

echo "node[2].face++ <--> { delay = 1ms; } <-->node[5].face++;
node[2].face++ <--> { delay = 1ms; } <-->node[6].face++;
node[3].face++ <--> { delay = 1ms; } <-->node[7].face++;
node[3].face++ <--> { delay = 1ms; } <-->node[8].face++;" >> ${outFile}

## 3 --> 9
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[3].face++ <--> { delay = 1ms; } <-->node[9].face++;" >> ${outFile}
fi
## 3 --> 10
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[3].face++ <--> { delay = 1ms; } <-->node[10].face++;" >> ${outFile}
fi
## 4 --> 7
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[4].face++ <--> { delay = 1ms; } <-->node[7].face++;" >> ${outFile}
fi
## 4 --> 8
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[4].face++ <--> { delay = 1ms; } <-->node[8].face++;" >> ${outFile}
fi

echo "node[4].face++ <--> { delay = 1ms; } <-->node[9].face++;
node[4].face++ <--> { delay = 1ms; } <-->node[10].face++;
node[5].face++ <--> { delay = 1ms; } <-->node[11].face++;
node[5].face++ <--> { delay = 1ms; } <-->node[12].face++;" >> ${outFile}

## 5 --> 13
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[5].face++ <--> { delay = 1ms; } <-->node[13].face++;" >> ${outFile}
fi
## 5 --> 14
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[5].face++ <--> { delay = 1ms; } <-->node[14].face++;" >> ${outFile}
fi
## 6 --> 11
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[6].face++ <--> { delay = 1ms; } <-->node[11].face++;" >> ${outFile}
fi
## 6 --> 12
numRand=`awk -v seed=$RANDOM 'BEGIN{srand(seed);printf("%.5f\n", rand())}'`
if [ `echo "$numRand<$conProb" | bc -l` -eq 1 ]
then
  echo "node[6].face++ <--> { delay = 1ms; } <-->node[12].face++;" >> ${outFile}
fi

echo "node[6].face++ <--> { delay = 1ms; } <-->node[13].face++;
node[6].face++ <--> { delay = 1ms; } <-->node[14].face++;
}" >> ${outFile};
done