#!/bin/bash

# This script 
# 	(i)   creates a new .ini file according to parameters passed through the launch_Parallel_ModelGraft.sh script,
#	(ii)  launch the Parallel ModelGraft simulation
#   (iii) collects and elaborate results. 

# PREREQUISITES (IMPORTANT)
#
# 	(1) Since this script will take the skeleton of the "Paralle_ModelGraft_Akaros.ini" file as a reference in order to 
# 		create new .ini files according to the simulated scenario, PLEASE DO NOT MODIFY IT. 
#	(2) This script is supposed to be executed from the server on which the Master is running (i.e., modelgraft1 in this example)
#		since it simplifies the post-elaboration part. 



#### Directories and commands definition #### (Please change according to your actual paths and server names)
user=modelgraft
userHome=/home/modelgraft
ccnSimDir=${userHome}/ccnSim-Parallel
akaroaBinDir=/usr/local/akaroa/bin


## ccnSim executable
main=${ccnSimDir}/ccnSim

## Output and other Directories
resultDir=${ccnSimDir}/results
infoDir=${ccnSimDir}/infoSim
logDir=${ccnSimDir}/logs
tcDir=${ccnSimDir}/Tc_Values

## Servers
server1=modelgraft1
server2=modelgraft2
server3=modelgraft3


## Command line parameters reading
params=( "$@" )
	
Topology=$(echo ${params[0]})			# Topology
TotNodes=$(echo ${params[1]})
NumClients=$(echo ${params[2]})			# Number of clients
NumRepos=$(echo ${params[3]})			# Number of repos
ForwStr=$(echo ${params[4]})			# Forwarding Strategy
MetaCaching=$(echo ${params[5]})		# Cache Decision Policy
ReplStr=$(echo ${params[6]})			# Cache Replacement Strategy (Parallel ModelGraft is based only on TTL)

if [[ $ReplStr == "ttl" ]]
	then
	SimType="TTL"
else
	SimType="ED"
fi

Alpha=$(echo ${params[7]})				# Zipf's Exponent
CacheDim=$(echo ${params[8]})			# Cache Size (expressed in number of contents)

if [[ $MetaCaching == "two_lru" ]] || [[ $MetaCaching == "two_ttl" ]]
	then
	NameCacheDim=$(echo ${params[9]})		# Name Cache Size (only for 2-TTL)
else
	NameCacheDim="0"
fi

TotalCont=$(echo ${params[10]})			# Catalog Cardinality
TotalReq=$(echo ${params[11]})			# Total Number of Requests
Lambda=$(echo ${params[12]})			# Request Rate for each client
ClientType=$(echo ${params[13]})		# Client Type
ContDistrType=$(echo ${params[14]})		# Content Distribution Type
Toff=$(echo ${params[15]})				# Toff multiplicative factor (i.e., Ton = k*Toff, only for ShotNoise model, not tested in this Parallel version)

runs=$(echo ${params[16]})				# Number of simulated runs

startType=$(echo ${params[17]})			# Start Mode ('hot' or 'cold')
fillType=$(echo ${params[18]})			# Fill Mode ('model' or 'naive')
checkNodes=$(echo ${params[19]})		# Percentage of nodes checked for stabilization (i.e., Yotta <= 1)

down=$(echo ${params[20]})				# Downscaling factor.

threads=$(echo ${params[21]})			# Number of Parallel Threads we intend to instantiate.
servers=$(echo ${params[22]})				# Number of Physical Server we intend to use.


# If the name of the Tc file is not provided, it will be searched inside the Tc_Values folder, according to the scenario parameters. 
if [[ $SimType == "TTL" ]]
	then
	TcFileTemp=$(echo ${params[23]})

	if [ -n "$TcFileTemp" ]					# User provided Tc file
		then
		TcFile=$TcFileTemp
	else
		TcFile=${tcDir}/tc_${Topology}_NumCl_${NumClients}_NumRep_${NumRepos}_FS_${ForwStr}_MC_${MetaCaching}_M_${TotalCont}_R_${TotalReq}_C_${CacheDim}_Lam_${Lambda}.txt
	fi

	if [[ $MetaCaching == "two_ttl" ]]
		then
		TcNameFileTemp=$(echo ${params[24]})		
		if [ -n "$TcNameFileTemp" ]						# User provided Tc file for Name Cache
			then
			TcNameFile=$TcNameFileTemp					
		else
			TcNameFile=${tcDir}/tc_${Topology}_NumCl_${NumClients}_NumRep_${NumRepos}_FS_${ForwStr}_MC_${MetaCaching}_M_${TotalCont}_R_${TotalReq}_C_${CacheDim}_Lam_${Lambda}_NameCache.txt
		fi
	else
		TcNameFile="0"
	fi
fi

## Computing StedyTime and other parameters for the Parallel execution of the ModelGraft technique
valueReq=`echo ${TotalReq} | sed -e 's/[eE]+*/\\*10\\^/'`  					# Reading scientific notation
steady=$(echo "$valueReq / (${threads}*${Lambda}*${NumClients})" | bc -l)
steadyINT=${steady/.*}

# TTL-based scenario (i.e., usually downscaled)
downValue=`echo ${down} | sed -e 's/[eE]+*/\\*10\\^/'`  			# Reading scientific notation
TotalContValue=`echo ${TotalCont} | sed -e 's/[eE]+*/\\*10\\^/'`  	# Reading scientific notation
CacheDimValue=`echo ${CacheDim} | sed -e 's/[eE]+*/\\*10\\^/'`  	# Reading scientific notation

CacheDimDown=$(echo "$CacheDimValue / (${downValue})" | bc)
TotalContDown=$(echo "$TotalContValue / (${downValue})" | bc)
TotalReqDown=$(echo "$valueReq / (${downValue})" | bc)

if [[ $SimType == "ED" ]]
	then
	NumCycles=1
fi

## We have to check if the requested Tc File is actually present in tcDir. 
# If NOT, we (i) randomly generate Tc Values in the interval [cache_size/100, cache_size], (ii) copy them in the respective file, and
# (iii) copy the file on all the servers (since simulations from different servers should be based on the same Tc File. 
# At the end of the simulation, of the file was randomly generated, we remove it (since we suppose $tcDir contains only Tc Files with correct values)
TcRandom=false
LowTc=$(echo "$CacheDimValue / 100" | bc)
HighTc=$(echo "$CacheDimValue / 1" | bc)
HighTcNameCache=$(echo "$CacheDimValue / 10" | bc)

if [ -f "$TcFile" ]
then
	echo "Tc file FOUND in Tc_Values folder"
else
	echo "Tc file NOT FOUND in Tc_Values folder. It will be created with RANDOM values"
	TcRandom=true
	shuf -i ${LowTc}-${HighTc} -n ${TotNodes} > ${TcFile}
	
	if [[ $MetaCaching == "two_ttl" ]] # In case of TWO_TTL
		then
		shuf -i ${LowTc}-${HighTcNameCache} -n ${TotNodes} > ${TcNameFile}
	fi
fi 

## Copy the Tc File on the remaining servers
scp ${TcFile} ${user}@${server2}:${tcDir}/
scp ${TcFile} ${user}@${server3}:${tcDir}/
if [[ $MetaCaching == "two_ttl" ]] # In case of TWO_TTL
	then
	scp ${TcNameFile} ${user}@${server2}:${tcDir}/
	scp ${TcNameFile} ${user}@${server3}:${tcDir}/
fi


echo "*** SIM Parameters ***"
echo "Topology  =  ${Topology}"
echo "NumClients  =  ${NumClients}"
echo "NumRepos  =  ${NumRepos}"
echo "FS  =  ${ForwStr}"
echo "MC  =  ${MetaCaching}"
echo "RS  =  ${ReplStr}"
echo "Sim TYPE  =  ${SimType}"
echo "Alpha  =  ${Alpha}"
echo "Lambda  =  ${Lambda}"
echo "Cache DIM  =  ${CacheDim}"
echo "NameCache DIM  =  ${NameCacheDim}"
echo "Catalog  =  ${TotalCont}"
echo "Requests  =  ${TotalReq}"
echo "Client Type  =  ${ClientType}"
echo "Content Distr Type  =  ${ContDistrType}"
echo "Toff  =  ${Toff}"
echo "Start Type  =  ${startType}" 
echo "Fill Type  =  ${fillType}"
echo "Yotta  =  ${checkNodes}"
echo "#Runs  =  ${runs}"
echo "Steady Time  =  ${steadyINT}"

echo "Downsizing factor  =  ${down}"


if [[ SimType == "TTL" ]]
	then
	echo "Tc file  =  ${TcFile}"
	echo "Tc Name file  =  ${TcNameFile}"
fi


## Beginning of each LINE needed to be replaced inside the .ini file  
numClientsLS="**.num_clients"
numReposLS="**.num_repos"

netLS="network"
fwLS="**.FS"
mcLS="**.DS"
rsLS="**.RS"
alphaLS="**.alpha"
cacheDimLS="**.C"
nameCacheDimLS="**.NC"
totContLS="**.objects"
lambdaLS="**.lambda"
clientTypeLS="**.client_type"
contentDistrTypeLS="**.content_distribution_type"
catAggrLS="**.perc_aggr"
steadyLS="**.steady"
toffLS="**.toff_mult_factor"

startTypeLS="**.start_mode"
fillModeLS="**.fill_mode"
checkedNodesLS="**.partial_n"

downLS="**.downsize"

tcLS="**.tc_file"
tcNameLS="**.tc_name_file"

outputVectorIniLS="output-vector-file"
outputScalarIniLS="output-scalar-file"

outputAkaroaLS="**.akSim_out_file"
binLS="**.bin_file"

numThreadsLS="**.num_threads"



## Modification of the .ini file 
now=$(date +"%H_%M_%S")		# Current Time
	
iniFileMaster=${ccnSimDir}/Parallel_ModelGraft_Akaroa.ini
iniFileFinal=${ccnSimDir}/${Topology}_${ContDistrType}_${HOSTNAME}_${now}.ini
	
`cp ${iniFileMaster} ${iniFileFinal}`          # 'iniFileFinal' is the .ini file this script will be working with.

# Topology
`awk -v v1="${netLS}" -v v2="${Topology}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Num Clients
`awk -v v1="${numClientsLS}" -v v2="${NumClients}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Num Repos
`awk -v v1="${numReposLS}" -v v2="${NumRepos}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Forwarding Strategy
`awk -v v1="${fwLS}" -v v2="${ForwStr}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Cache Decision Policy
`awk -v v1="${mcLS}" -v v2="${MetaCaching}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Replacement Strategy
`awk -v v1="${rsLS}" -v v2="${ReplStr}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Alpha
`awk -v v1="${alphaLS}" -v v2="${Alpha}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`  

# Cache Size
`awk -v v1="${cacheDimLS}" -v v2="${CacheDim}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}` 

# Name Cache Size (2-LRU)
`awk -v v1="${nameCacheDimLS}" -v v2="${NameCacheDim}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Total Contents
`awk -v v1="${totContLS}" -v v2="${TotalCont}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Lambda
`awk -v v1="${lambdaLS}" -v v2="${Lambda}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Client Type
`awk -v v1="${clientTypeLS}" -v v2="${ClientType}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Content Distribution Type
if [[ $ClientType == "IRM"  ||  $ClientType == "Window" ]]
	then
	`awk -v v1="${contentDistrTypeLS}" -v v2="#${contentDistrTypeLS}" '$1==v1{$1='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
   	`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`
else
	`awk -v v1="${contentDistrTypeLS}" -v v2="${ContDistrType}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
   	`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`
fi

# Toff multiplicative factor
`awk -v v1="${toffLS}" -v v2="${Toff}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Steady Time
`awk -v v1="${steadyLS}" -v v2="${steadyINT}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Start Mode
`awk -v v1="${startTypeLS}" -v v2="${startType}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Fill Mode
`awk -v v1="${fillModeLS}" -v v2="${fillType}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Checked Nodes
`awk -v v1="${checkedNodesLS}" -v v2="${checkNodes}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

# Downsizing Factor
`awk -v v1="${downLS}" -v v2="${down}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

if [[ $SimType == "TTL" ]]
	then
	# Tc File Name
	`awk -v v1="${tcLS}" -v v2="${TcFile}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
	`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

	# Tc Name Cache File Name
	`awk -v v1="${tcNameLS}" -v v2="${TcNameFile}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
	`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`
fi
	
## Definition of Output Strings (pretty long but they help to immediately identify the scenario)
## and modification of the correspondent line inside the .ini file. 
outString=${SimType}_T_${Topology}_NumCl_${NumClients}_NumRep_${NumRepos}_FS_${ForwStr}_MC_${MetaCaching}_RS_${ReplStr}_C_${CacheDim}_NC_${NameCacheDim}_M_${TotalCont}_Req_${TotalReq}_Lam_${Lambda}_A_${Alpha}_CT_${ClientType}_ToffMult_${Toff}_Start_${startType}_Fill_${fillType}_ChNodes_${checkNodes}_Down_${down}_Threads_${threads}_Servers_${servers}_Now_${now}

`awk -v v1="${outputVectorIniLS}" -v v2="\$"{resultdir}"/${outString}_run=\$"{repetition}".vec" '$1==v1{$3='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

`awk -v v1="${outputScalarIniLS}" -v v2="\$"{resultdir}"/${outString}_run=\$"{repetition}".sca" '$1==v1{$3='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`


## Write number of threads into ini file
finalThreads=$threads

`awk -v v1="${numThreadsLS}" -v v2="${finalThreads}" '$1==v1{$5='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`



## Execute Parallel ModelGraft Simulations
for i in `seq 0 $runs`
do
	## Debug file with Akaroa simulation  (Change the final .ini file for each run)
	`awk -v v1="${outputAkaroaLS}" -v v2="${logDir}/${outString}_run=${i}" '$1==v1{$6='v2'}{print $0}' ${iniFileFinal} > ${iniFileFinal}_temp.ini`
	`mv ${iniFileFinal}_temp.ini ${iniFileFinal}`

	##### COPY the .ini file on the remaining servers
	scp ${iniFileFinal} ${user}@${server2}:${ccnSimDir}/
	scp ${iniFileFinal} ${user}@${server3}:${ccnSimDir}/
   	
   	###
   	# Options for Akaroa
   	#  -d 			= turns debugging on.
   	#  -n $threads	= specifies the number of threads that we intend to instantiate.
   	#  -D  			= used to pass Akaroa environment variables. 
	# ParMode=Sync  = specifies that the checkpoint processing should be synchronous, i.e., the master node will wait
	#				  for the reception of the checkpoint vectors from all the instantiated threads.  

	${akaroaBinDir}/akrun -d -n ${finalThreads} -D ParMode=Sync -- $main -u Cmdenv -f $iniFileFinal -r $i 
	###

done

sleep 2

## Remove Tc Files if randomly generated 
if [ "$TcRandom" = true ] ; then
    rm ${TcFile}
    ssh ${server2} -l ${user} "rm ${TcFile}"
    ssh ${server3} -l ${user} "rm ${TcFile}" 
    if [[ $MetaCaching == "two_ttl" ]] # In case of TWO_TTL
		then
		rm ${TcNameFile}
    	ssh ${server2} -l ${user} "rm ${TcNameFile}"
    	ssh ${server3} -l ${user} "rm ${TcNameFile}" 
    fi
fi

## Collect Output files and Compute KPIs

# All the log files and copied in the same folder on the Master node (ccnSim is supposed to has the same installation path on all the servers)
scp ${user}@${server2}:${logDir}/* ${logDir}/
scp ${user}@${server3}:${logDir}/* ${logDir}/

scp ${user}@${server2}:${resultDir}/* ${resultDir}/
scp ${user}@${server3}:${resultDir}/* ${resultDir}/


# With Parallel ModelGrat Hits and Miss events (per each node) from each thread should be summed in order to compute the total HIT rate.
for i in `seq 0 $runs`
do
	# HIT rate
	# We retrieve the number and the ID of the active nodes first
	grep -h "\*\* NODE" ${logDir}/${outString}_run=${i}* | awk '{print $4}' | sed 's/:$//' >> ${resultDir}/ACTIVE_NODES_ALL_${outString}_ENGINES_RUN_${i}
	numActivesTot=$(cat ${resultDir}/ACTIVE_NODES_ALL_${outString}_ENGINES_RUN_${i} | wc -l)
	numActivesSingle=$(echo "$numActivesTot / ${threads}" | bc)
	head -n $numActivesSingle -q ${resultDir}/ACTIVE_NODES_ALL_${outString}_ENGINES_RUN_${i} > ${resultDir}/ACTIVE_NODES_SINGLE_${outString}_ENGINES_RUN_${i}
	while read NodeID
	do
    	grep -h "\*\* NODE \# ${NodeID}:" ${logDir}/${outString}_run=${i}* | awk '{sumA+=$7;sumB+=$10} END {print sumA/(sumA+sumB)}' >> ${resultDir}/HIT_${outString}_ENGINES_RUN_${i}
    	grep -h "\*\*(stable) NODE \# ${NodeID}:" ${logDir}/${outString}_run=${i}* | awk '{sumA+=$7;sumB+=$10} END {print sumA/(sumA+sumB)}' >> ${resultDir}/HIT_STABLE_${outString}_ENGINES_RUN_${i}
	done < ${resultDir}/ACTIVE_NODES_SINGLE_${outString}_ENGINES_RUN_${i}

	# Measured CACHE
	grep -h "\*\* CACHE-NODE" ${logDir}/${outString}_run=${i}* | awk '{print $4}' | sed 's/:$//' >> ${resultDir}/ALL_NODES_${outString}_ENGINES_RUN_${i}
	numTot=$(cat ${resultDir}/ALL_NODES_${outString}_ENGINES_RUN_${i} | wc -l)
	numSingle=$(echo "$numTot / ${threads}" | bc)
	head -n $numSingle -q ${resultDir}/ALL_NODES_${outString}_ENGINES_RUN_${i} > ${resultDir}/ALL_NODES_SINGLE_${outString}_ENGINES_RUN_${i}
	while read NodeID
	do
		# MEAN of the different measured caches collected by all the threads
    	grep -h "\*\* CACHE-NODE \# ${NodeID}:" ${logDir}/${outString}_run=${i}* | awk '{x[NR]=$7; s+=$7} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR); print a,sd}' >> ${resultDir}/CACHE_${outString}_ENGINES_RUN_${i}
	done < ${resultDir}/ALL_NODES_SINGLE_${outString}_ENGINES_RUN_${i}

	# Execution Time (both Stabilization and End)
	grep "Execution time of the STABILIZATION" ${logDir}/${outString}_run\=${i}* | awk '{print $7}' >> ${resultDir}/CPU_STABLE_${outString}_ENGINES_RUN_${i}
	grep "Execution time of the SIMULATION" ${logDir}/${outString}_run\=${i}* | awk '{print $7}' >> ${resultDir}/CPU_END_${outString}_ENGINES_RUN_${i}
	grep -h "SIMULATION ENDED AT CYCLE" ${logDir}/${outString}_run\=${i}* | awk '{print $6}' >> ${resultDir}/SIM_CYCLES_${outString}_ENGINES_RUN_${i}
	grep -h "Distance" ${logDir}/${outString}_run\=${i}* | awk '{print $2}' >> ${resultDir}/DISTANCE_${outString}_ENGINES_RUN_${i}
	grep "MEMORY" ${logDir}/${outString}_run\=${i}* | awk '{print $3}' >> ${resultDir}/MEMORY_${outString}_ENGINES_RUN_${i}
	

	## Average Metrics for each run (computed by averaging the results collected from all the engines)
	# For the "execution time", however, we need to keep the maximum value
	cat ${resultDir}/CPU_STABLE_${outString}_ENGINES_RUN_${i} | sort -nr | head -n1 >> ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN_RUN_${i}
	cat ${resultDir}/CPU_END_${outString}_ENGINES_RUN_${i} | sort -nr | head -n1 >> ${resultDir}/CPU_END_${outString}_ENGINES_MEAN_RUN_${i}
	
	awk '{sum+=$1} END { print sum}' ${resultDir}/CACHE_${outString}_ENGINES_RUN_${i} >> ${resultDir}/CACHE_${outString}_ENGINES_SUM_RUN_${i}
	awk '{sum+=$1} END { print sum/NR}' ${resultDir}/HIT_${outString}_ENGINES_RUN_${i} >> ${resultDir}/HIT_${outString}_ENGINES_MEAN_RUN_${i}
	awk '{sum+=$1} END { print sum/NR}' ${resultDir}/HIT_STABLE_${outString}_ENGINES_RUN_${i} >> ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN_RUN_${i}
	awk '{sum+=$1} END { print sum/NR}' ${resultDir}/DISTANCE_${outString}_ENGINES_RUN_${i} >> ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN_RUN_${i}
	awk '{sum+=$1} END { print sum/NR}' ${resultDir}/SIM_CYCLES_${outString}_ENGINES_RUN_${i} >> ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN_RUN_${i}
	# For the MEMORY we should print the sum of all the engines (than it depends on how many hosts the simulation is distributed on)
	awk '{sum+=$1} END { print sum}' ${resultDir}/MEMORY_${outString}_ENGINES_RUN_${i} >> ${resultDir}/MEMORY_${outString}_ENGINES_MEAN_RUN_${i}

	# Move cache measurement in the right folder
	mv ${resultDir}/CACHE_${outString}_ENGINES_RUN_${i} ${resultDir}/parallel/CACHE_${outString}_RUN_${i}
done

## Now we can average results from all the runs
head -n 1 -q ${resultDir}/CACHE_${outString}_ENGINES_SUM_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/CACHE_${outString}_ENGINES_SUM_MEAN
head -n 1 -q ${resultDir}/HIT_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/HIT_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/CPU_END_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/CPU_END_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}' >> ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/MEMORY_${outString}_ENGINES_MEAN_RUN* | awk '{sum+=$1} END { print sum/NR}'  >> ${resultDir}/MEMORY_${outString}_ENGINES_MEAN

head -n 1 -q ${resultDir}/CACHE_${outString}_ENGINES_SUM_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/CACHE_${outString}_ENGINES_SUM_MEAN
head -n 1 -q ${resultDir}/HIT_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/HIT_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/CPU_END_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/CPU_END_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}' >> ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN
head -n 1 -q ${resultDir}/MEMORY_${outString}_ENGINES_MEAN_RUN* | awk '{x[NR]=$1; s+=$1} END{a=s/NR; for (i in x){ss += (x[i]-a)^2} sd = sqrt(ss/NR) ;conf=1.860*(sd/sqrt(NR)); print sd; print conf}'  >> ${resultDir}/MEMORY_${outString}_ENGINES_MEAN



### Computing Simulation Performance 
# Gather all the metrics (mean values) in one file
cacheSumStr=$(head -n 1 ${resultDir}/CACHE_${outString}_ENGINES_SUM_MEAN)
hitEndStr=$(head -n 1 ${resultDir}/HIT_${outString}_ENGINES_MEAN)
hitStabStr=$(head -n 1 ${resultDir}/HIT_STABLE_${outString}_ENGINES_MEAN)
distanceStr=$(head -n 1 ${resultDir}/DISTANCE_${outString}_ENGINES_MEAN)
cpuStabStr=$(head -n 1 ${resultDir}/CPU_STABLE_${outString}_ENGINES_MEAN)
NumCycles=$(head -n 1 ${resultDir}/SIM_CYCLES_${outString}_ENGINES_MEAN)
cpuEndStr=$(head -n 1 ${resultDir}/CPU_END_${outString}_ENGINES_MEAN)
memoryStr=$(head -n 1 ${resultDir}/MEMORY_${outString}_ENGINES_MEAN)


# CPU in seconds
cpuStab=`echo "scale=2; $cpuStabStr / 1000" | bc`
cpuEnd=`echo "scale=2; $cpuEndStr / 1000" | bc`

# Memory in MB
memory=`echo "scale=2; $memoryStr / 1048576" | bc`

NumNodes=$(grep -h "Num Total Nodes" ${logDir}/${outString}_run\=0* | head -n 1 | awk '{print $4}')

header_str="Topo\tNodes\tClients\tRepos\tAlpha\tLambda\tMC\tRP\tFS\tM\tC\tR\tDelta\tM'\tC'\tR'\tYotta\tp_hit_stab\tp_hit_end\thit_dist\tMem[MB]\tCPU_stab[s]\tCPU_end[s]\tCycles\tSimType\tStartType\tFillType\tSumMeasCache\n"
final_str="${Topology}\t${NumNodes}\t${NumClients}\t${NumRepos}\t${Alpha}\t${Lambda}\t${MetaCaching}\t${ReplStr}\t${ForwStr}\t${TotalCont}\t${CacheDim}\t${TotalReq}\t${down}\t${TotalContDown}\t${CacheDimDown}\t${TotalReqDown}\t${checkNodes}\t${hitStabStr}\t${hitEndStr}\t${distanceStr}\t${memory}\t${cpuStab}\t${cpuEnd}\t${NumCycles}\t${SimType}\t${startType}\t${fillType}\t${cacheSumStr}\n"

echo -e $header_str > ${infoDir}/ALL_MEASURES_${outString}
echo -e $final_str >> ${infoDir}/ALL_MEASURES_${outString}

mv ${infoDir}/ALL_MEASURES_${outString} ${resultDir}/parallel/ALL_MEASURES_${outString}

## (IMPORTANT) All the single output files are removed from every server, and only the SUMMARY file "ALL_MEASURES*" is kept in ${resultDir}/parallel folder.
##			   If you want to keep them, please comment the following lines.
rm ${logDir}/*
ssh -f ${server2} -l ${user} "source ~/.bashrc ; rm ${logDir}/*"
ssh -f ${server3} -l ${user} "source ~/.bashrc ; rm ${logDir}/*"

rm ${resultDir}/*
ssh -f ${server2} -l ${user} "source ~/.bashrc ; rm ${resultDir}/*"
ssh -f ${server3} -l ${user} "source ~/.bashrc ; rm ${resultDir}/*"

rm ${infoDir}/*
ssh -f ${server2} -l ${user} "source ~/.bashrc ; rm ${infoDir}/*"
ssh -f ${server3} -l ${user} "source ~/.bashrc ; rm ${infoDir}/*"

rm ${resultDir}/parallel/CACHE*
