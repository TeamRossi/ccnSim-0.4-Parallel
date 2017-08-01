/*
 * ccnSim is a scalable chunk-level simulator for Content Centric
 * Networks (CCN), that we developed in the context of ANR Connect
 * (http://www.anr-connect.org/)
 *
 * People:
 *    Giuseppe Rossini (Former lead developer, mailto giuseppe.rossini@enst.fr)
 *    Raffaele Chiocchetti (Former developer, mailto raffaele.chiocchetti@gmail.com)
 *    Andrea Araldo (Principal suspect 1.0, mailto araldo@lri.fr)
 *    Michele Tortelli (Principal suspect 1.1, mailto michele.tortelli@telecom-paristech.fr)
 *    Dario Rossi (Occasional debugger, mailto dario.rossi@enst.fr)
 *    Emilio Leonardi (Well informed outsider, mailto emilio.leonardi@tlc.polito.it)
 *
 * Mailing list: 
 *	  ccnsim@listes.telecom-paristech.fr
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <cmath>
#include <unistd.h>
#include "statistics.h"
#include "core_layer.h"
#include "base_cache.h"
#include "strategy_layer.h"
#include "content_distribution.h"
#include "ShotNoiseContentDistribution.h"
#include "lru_cache.h"
#include <unistd.h>
#include "ccn_data.h"
#include "always_policy.h"
#include "fix_policy.h"
#include "two_lru_policy.h"
#include "two_ttl_policy.h"
#include "client_IRM.h"
#include "ttl_cache.h"
#include "ttl_name_cache.h"

#include "error_handling.h"

// AKAROA
#include <akaroa.H>
#include <akaroa/ak_message.H>

// SIGNALS
#include<stdio.h>
#include<signal.h>
#include<unistd.h>

// MEMORY
#include <unistd.h>
#include <sys/resource.h>


Register_Class(statistics);

#if OMNETPP_VERSION >= 0x0500
    using namespace omnetpp;
#endif


using namespace std;


extern "C" double compute_Tc_single_Approx(double cSizeTarg, double alphaVal, long catCard, float** reqRates, int colIndex, const char* decString, double fixProb);
extern "C" double compute_Tc_single_Approx_More_Repo(double cSizeTarg, double alphaVal, long catCard, float** reqRates, int colIndex, const char* decString, double fixProb);
extern "C" void signal_handler(int sigType);

bool stop_simulation = false;
bool signal_received = false;
bool steadyState = false;			// It is used when SIGUSR* are received.

void signal_handler(int signo)
{
  switch(signo){
  case 30:				 // SIGUSR1 = Continue
	  if(!steadyState)   // Still in the "Transient Phase"
	  {
		  // The Master node has communicated that the transient phase should continue since the
		  // centralized stable check (done at the Master) has provided a negative outcome.
		  char *msg1 = (char *)"SIGUSR1 was CATCHED!! CONTINUE the CENTRALIZED STABLE CHECK!\n";
		  AkMessage(msg1);
		  steadyState = false;
		  signal_received = true;
		  stop_simulation = false;
	  }
	  else				// The simulation is already at "steady-state" and waiting to receive a STOP signal from the Master.
	  {
		  // The Master node has communicated that the ModelGraft "consistency check" has provided a negative outcome,
		  // and thus the simulation should continue with another MC-TTL cycle.
		  char *msg2 = (char *)"SIGUSR1 was CATCHED!! CONTINUE the simulation with another MC-TTL cycle!\n";
		  AkMessage(msg2);
		  steadyState = false;
		  signal_received = true;
		  stop_simulation = false;
	  }
	  break;
  case 31:					// SIGUSR1 = Stop
	  if(!steadyState)		// Still in the "Transient Phase"
	  {
		  // The Master node has communicated that the transient phase should be stopped since the
		  // centralized stable check (done at the Master) has provided a positive outcome.
		  char *msg3 = (char *)"SIGUSR2 was CATCHED!! STOP the CENTRALIZED STABLE CHECK!\n";
		  AkMessage (msg3);
		  steadyState = true;
		  signal_received = true;
		  stop_simulation = false;
	  }
	  else				// The simulation is already at "steady-state" and waiting to receive a STOP signal from the Master.
	  {
		  // The Master node has communicated that the ModelGraft "consistency check" has provided a positive outcome,
		  // and thus the simulation can be stopped.
		  char *msg4 = (char *)"SIGUSR2 was CATCHED!! STOP the simulation\n";
		  AkMessage (msg4);
		  signal_received = true;
		  stop_simulation = true;
	  }
  	  break;
  default:
	  char *msg5 = (char *)"THE RECEIVED SIGNAL IS UNKNOWN!!\n";
	  AkMessage(msg5);
  }
}

void statistics::dbgWithAkaroa(const char* dbgMsg)
{
	dbAk << dbgMsg;
}

void statistics::init_debug_file()
{
	// We have to insert something that differentiates the output according to HOST and ENGINE
	// that run the simulation.
	debugFilePath = par("akSim_out_file");

	char hostname[100];
	int pid;
	gethostname(hostname, 100);
	pid = getpid();

	stringstream full_out_str;
	string out_str = ".out";

	full_out_str << debugFilePath << "_HOSTNAME_" << hostname << "_PID_" << pid << out_str;

	dbAk.open (full_out_str.str().c_str(), ofstream::out);
}

void statistics::initialize(int stage)
{
	if(stage == 1)
	{
		// *** AKAROA INITIALIZATION ***

		int numParam = 2;			// We consider the HitMiss vector for the stability, and the measured cache size at the end.
		AkDeclareParameters(numParam);  // Declare the number of parameters

		char *msg6 = (char *)"Parameters declared:  %i";
		AkMessage(msg6, numParam);

		// Initialize signal handlers
		char *msg7 = (char *)"-- INITIATING SIGNLAL HANDLERs!!\n";
		AkMessage(msg7);

		if (signal(30, signal_handler) == SIG_ERR)		// Signal = Continue (both for stability and end of simulation)
		{
			char *msg8 = (char *)"\ncan't catch SIGUSR1\n";
			AkMessage(msg8);
		}
		if (signal(31, signal_handler) == SIG_ERR)		// Signal = Stop (both for stability and end of simulation)
		{
			char *msg9 = (char *)"\ncan't catch SIGUSR2\n";
			AkMessage(msg9);
		}

		// **********************************

		scheduleEnd = false;
		tStartGeneral = chrono::high_resolution_clock::now();
		dbAk << "Initialize STATISTICS...\tTime:\t" << SimTime() << "\n";


		// Topology info
		num_nodes 	= getAncestorPar("n");
		num_clients = getAncestorPar("num_clients");
		num_repos = getAncestorPar("num_repos");
   
		// Statistics parameters
		ts = par("ts"); 		// Sampling time
		window = par("window");

		num_threads = par("num_threads");
		//engineWindow = floor(window * 1./num_threads);    // We need to send this sizeVect to the master
		if(num_threads == 1)
			engineWindow = window;    // We need to send this sizeVect to the master
		else
			engineWindow = floor(window/num_threads);    // We need to send this sizeVect to the master

		if(engineWindow < 10)
		{
			engineWindow = 10;				// We put a lower bound, other engineWindow would be too small.
			ts = num_nodes * 6;
		} else{
			ts = num_nodes * 3;
		}



		dbAk << "NUMBER OF CORES:\t" << num_threads << endl;
		dbAk << "ENGINE WINDOW SIZE:\t" << engineWindow << endl;

		/* Init vectors that are sent to the Master for the Centralized Stable Check.
		 * Each node will collect 2*engineWindow samples, i.e., 'engineWindow' Hit and Miss measures.
		 * The size of the whole vector sent to the Master from each Engine will be 'num_nodes*2*engineWindow'
		 * The collection of samples will be executed each "Ts" seconds; if a valid sample is not available,
		 * the default "-1" value will be left, meaning that no sample has been collected.
		*/
		sizeVect = num_nodes*2*engineWindow;
    	HitMiss = new int[sizeVect];
    	for(int i=0; i<sizeVect; i++)
    		HitMiss[i] = -1;				// Initialized to '-1', which means 'sample not collected'

    	// Initialize the counter of samples (used to trigger HitMiss vector sending)
    	sample_counter.resize(num_nodes);
    	for (int j=0; j<num_nodes; j++)
    	{
    		sample_counter[j] = 0;
    	}


		// Only model solver of entire simulation
		onlyModel = par("onlyModel");

		// If the Shot Noise Model is simulated, the steady state time is evaluated
		// according to the parameters extracted from the configuration file, and to the total
		// number of requests that the user wants to simulate. The initialization stage of Statistics module
		// is after the one of ShotNoiseContentDistribution class.
		cModule* pSubModule = getParentModule()->getSubmodule("content_distribution");
		if (pSubModule)
		{
			ShotNoiseContentDistribution* pClass2Module = dynamic_cast<ShotNoiseContentDistribution*>(pSubModule);
			if (pClass2Module)		// The Shot Noise Model is simulated.
			{
				time_steady = pClass2Module->steadySimTime;
			}
			else					// The IRM model is simulated. Read the steady time from the .ini file.
				time_steady = par("steady");
		}

		dbAk << "STEADY_TIME: " << time_steady << endl;

		string startMode = par("start_mode");       // Hot vs Cold start.
		string fillMode = par("fill_mode");			// Naive vs Model.

		// Downscaling factor. In case of ED-SIM: downsize = 1.

		long unsigned int downsize_temp = par("downsize");
		cout << " ------- DOWNSIZE from Statistics Par:\t" << downsize_temp << " -------" << endl;
		downsize = (long long int)downsize_temp;
		cout << " ------- DOWNSIZE from Statistics After conversion:\t" << downsize << " -------" << endl;

		dbAk << "*** DOWNSIZE VALUE: " << downsize << endl;

		partial_n = par("partial_n");		// Number of nodes whose state will be checked for the intelligent transient state monitor.

		// Coefficient of Variation (CV) and Consistency thresholds.
        cvThr = par("cvThr");
        consThr = par("consThr");

		if (partial_n < 0 || partial_n > 1)
		{
			std::stringstream ermsg;
			ermsg<<"The parameter \"partial_n\" indicates the proportion of nodes to be considered!"
					"It must be in the range [0,1]. Please correct.";
			severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
		}
		partial_n = round(partial_n*(double)num_nodes);


		cTopology topo;

		//	Extracting client modules according to the simulated type (IRM or ShotNoise).
		string clType = getAncestorPar("client_type");
		string clBase = "modules.clients.";
		clBase+=clType;
		clients = new client* [num_clients];
		clients_id.resize(num_clients);
		vector<string> clients_vec(1,clBase);
		topo.extractByNedTypeName(clients_vec);
		int k = 0;
		for (int i = 0;i<topo.getNumNodes();i++)
		{
		    int c = ((client *)topo.getNode(i)->getModule())->getNodeIndex();
		    if (find (content_distribution::clients, content_distribution::clients + num_clients, c)
			!= content_distribution::clients + num_clients)
		    {
		    	clients[k] = (client *)topo.getNode(i)->getModule();
		    	clients_id[k] = c;
		    	k++;
		    }
		}
    
		total_replicas = *(content_distribution::total_replicas_p );

		if ( content_distribution::repo_popularity_p != NULL )
		{
			for (unsigned repo_idx =0; repo_idx < content_distribution::repo_popularity_p->size(); repo_idx++)
			{
				char name[30];

				sprintf ( name, "repo_popularity[%u]",repo_idx);
				double repo_popularity = (*content_distribution::repo_popularity_p)[repo_idx];
				recordScalar(name,repo_popularity);
			}


			#ifdef SEVERE_DEBUG
				double repo_popularity_sum = 0;
				for (unsigned repo_idx =0; repo_idx < content_distribution::repo_popularity_p->size();
					 repo_idx++)
				{
					repo_popularity_sum += (*content_distribution::repo_popularity_p)[repo_idx];
				}

				if ( ! double_equality(repo_popularity_sum, 1) )
				{
					std::stringstream ermsg;
					ermsg<<"pop_indication_sum="<<repo_popularity_sum<<
						". It must be 1";
					severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
				}
			#endif
		} // else info about repo popularity are not calculated


		// Extracting cache and core modules.
		caches = new base_cache*[num_nodes];
		cores = new core_layer*[num_nodes];
		vector<string> nodes_vec(1,"modules.node.node");
		topo.extractByNedTypeName(nodes_vec);

		for (int i = 0;i<topo.getNumNodes();i++)
		{
			//caches[i] = (base_cache *) (topo.getNode(i)->getModule()->getModuleByPath("content_store"));
			caches[i] = (base_cache *) (topo.getNode(i)->getModule()->getSubmodule("content_store"));
			//cores [i] = (core_layer *) (topo.getNode(i)->getModule()->getModuleByPath("core_layer"));
			cores [i] = (core_layer *) (topo.getNode(i)->getModule()->getSubmodule("core_layer"));
		}

		//	Store samples for stabilization
		samples.resize(num_nodes);
		events.resize(num_nodes);
		stable_nodes.resize(num_nodes);
		stable_with_traffic_nodes.resize(num_nodes);
		for (int n=0; n < num_nodes; n++)
		{
			events[n] = 0;
			stable_nodes[n] = false;
			stable_with_traffic_nodes[n] = false;
		}


		// Initialize the state of the simulation. This will be used when SIGUSR* signals are received.
		// e.g. when steadyState = false -> CONTINUE means continue centralized_stable_check
		//								 -> END means enter the steadyState and stop the centralized_stable_check
		//      when steadyState = true  -> CONTINUE means that we need another MC-TTL cycle
		//								 -> END stops the simulation
		steadyState = false;


		centralized_stable_check = new cMessage("centralized_stable_check",CENTRALIZED_STABLE_CHECK);
		end = new cMessage("end",END);

		dbAk << endl;

		string replStr = caches[0]->getParentModule()->par("RS");
		dbAk << "REPLACEMENT STRATEGY:\t" << replStr << endl;
		if(replStr.compare("ttl_cache") != 0)
		{
			std::stringstream ermsg;
			ermsg<<"Parallelization is possible only with TTL caches! Please correct...";
			severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
		}
		else
		{
			scheduleAt(simTime() + ts, centralized_stable_check);  // Schedule a centralized stable check.
		}
	}
}

int statistics::numInitStages() const
{
	return 2;
}

/*
 * 	Handle timers aimed at checking the state of the simulation, e.g.,
 * 	if HitMiss vector can be sent to Master, or End of the simulation.
 */
void statistics::handleMessage(cMessage *in)
{
    int stables = 0;
    int numActiveNodes = 0;
    double phitNode;
    double phitTot = 0;
    int pid = 0;
    int numValidNodes = 0;
    int paramNum;

    switch (in->getKind()){

    case CENTRALIZED_STABLE_CHECK:
    	for (int i = 0;i<num_nodes;i++)
    	{
 			centralized_stable(i);				// Collect only valid samples from each node.
    	}

    	// The HitMiss vector will be sent to the Master after the first "partial_n" nodes have collect engineWindow Hit and Miss samples.
    	// In this way we avoid that peripheral nodes (or the ones that are not traversed by a relevant traffic) could
    	// slow down the entire process of sampling collection and sending to the Master.
    	// The whole sent vector might, then, partially contain also default values (i.e., '-1') for some nodes.

    	// Indeed, checks like "stables >= partial_n" and "stable_with_traffic >= floor(partial_n/2)" will be done by the Master.
    	// stable_nodes[]=true/false should be, as a consequence, set from the Master in order to do the control above.
    	// Moreover, "caches[]->stability", "clients[]->stability" should be set after having received the right signal from the Master.

    	for (int j=0; j<num_nodes; j++)
    	{
    		if(sample_counter[j] == engineWindow)
    			numValidNodes++;
    	}

    	if(numValidNodes >= partial_n)
    	{
			// *** AKAROA *** Observations are sent to the Master and marked with the pid = process ID of the current engine.
			pid = getpid();
			paramNum = 2;


			/* // Can be used for Debugging
			dbAk << "I'm sending these samples:" << endl;
			for (int j=0; j<num_nodes; j++)
			{
				for (int z=0; z<engineWindow; z++)
				{
					if(HitMiss[(j*(int)engineWindow)+z] > 0)
						dbAk << "PID # " << pid << "\t NODE # " << j << "\t SAMPLE # " << z << "\t VALUE - "<< HitMiss[(j*(int)engineWindow)+z] << endl;
				}
			}*/

			/*
			 *   Function used to send the HitMiss vector to the Master
			 *
			 *   Param:
			 *   - paramNum: it specifies the number associated to the the checkpoint (metric) that we are sending to the Mastr.
			 *   			 In out case, for the HitMiss vector, paramNum = 2, while for the MeasCacheSize sent at the and of the
			 *   			 simulation it will be paramNum = 1. This number will be used by the Master to differentiate the
			 *   			 checkpoint processing.
			 *   - HitMiss: it is the  vector with the collected samples for all the nodes.
			 *   - num_nodes: it specifies the total number of nodes.
			 *   - engineWindow: size of the window of samples for each node.
			 *   - partial_n: actual number of nodes for which the check will be done (usually < numNodes).
			 *   - pid = process id of the current engine.
			 *   - num_threads = number of parallel threads which are executing the current simulation.
			 */
			AkObservationMGvect(paramNum, HitMiss, num_nodes, (int)engineWindow, partial_n, pid, num_threads);

			// Waiting for any Signal to be received from the Master
			while(!signal_received)
			{
				sleep(1);
			}

			sleep(1);

			if(steadyState)  // It means that the Master has 'stated' the steady-state after the centralized stable check.
			{
				// The signal_received is set again to 'false' in order to wait for the 'end_of_sim' signal.
				signal_received = false;

				// Set flags for caches and clients
				for (int i = 0;i<num_nodes;i++)
					caches[i]->stability = true;

				for(int i=0; i<num_clients; i++)
					clients[i]->stability = true;

				dbAk << "*** FULL STABLE ***" << endl;

				// Compute HIT rate for active nodes on the current Engine.
				// (It does not make much sense here, since it is the Master that should do this with samples from all the engines
				//	It can be used to keep track of the different system evolutions for the different Engines).
				phitTot = 0;
				numActiveNodes = 0;
				for (int i=0; i < num_nodes; i++)
				{
					if(cores[i]->interests != 0)
					{
						dbAk << "**(stable) NODE # " << i << ":\tHITS = " << caches[i]->hit << "\tMISS = " << caches[i]->miss << endl;
						phitNode = caches[i]->hit * 1./ ( caches[i]->hit + caches[i]->miss );
						phitTot += phitNode;
						dbAk << "Node # " << i << " pHit: " << phitNode << endl;
						phitNode = 0;
						numActiveNodes++;
						cores[i]->stable = true;
					}
					else
					{
						phitTot += 0;
						cores[i]->stable = true;
					}
				}

				dbAk << "SIMULATION - Total MEAN HIT PROB AFTER STABILIZATION: " << phitTot * 1./(double)numActiveNodes << endl;
				dbAk << "Number of Active Nodes: " << numActiveNodes << endl;
				dbAk << "Simulation - STABILIZATION reached at: " << simTime() << endl;  // Simulation time

				// ** Actual CPU time for stability measured since the beginning
				tEndStable = chrono::high_resolution_clock::now();
				auto duration = chrono::duration_cast<chrono::milliseconds>( tEndStable - tStartGeneral ).count();
				dbAk << "Execution time of the STABILIZATION [ms]: " << duration << endl;

				stability_has_been_reached();

				for (int n=0; n < num_nodes; n++)
					events[n] = 0;

				// Schedule the END of the simulation according to the 'time_steady' (i.e., number of requests) after the stabilization
				scheduleAt(simTime() + time_steady, end);

				// In case of many parallel threads (i.e., > 70), the time_steady might be too small (i.e., even smaller than the stability time)
				// This can cause a premature ending of the simulation and the necessity to execute another MC-TTL cycle.
				// It is still an open problem how to solve this.
				// A workaround (that does not seem to work) might be :

				/*if( time_steady < SIMTIME_DBL(simTime()) )  // time_steady is too small (simTime() represents here the stabilization time)
					scheduleAt(simTime() + SIMTIME_DBL(simTime()), end);
				else						// schedule the simulation end at time_steady.
					scheduleAt(simTime() + time_steady, end);
				*/
			}
			else  // The Master has 'stated' that the centralized stable check must continue by collecting other Hit and Miss samples.
			{
				signal_received = false;

				for(int i=0; i<sizeVect; i++)	// Reset the HitMiss vector
					HitMiss[i] = -1;

				// Reset sample_counter for each node.
				for (int j=0; j<num_nodes; j++)
					sample_counter[j] = 0;

				scheduleAt(simTime() + ts, in);		// Reschedule a centralized_stable_check.
			}
    	} // Not enough nodes have collected the valid amount of samples.
    	else
    		scheduleAt(simTime() + ts, in);		// Reschedule a centralized_stable_check.

	    break;

    case END:		// The scheduled END of the simulation has come.

    	tEndGeneral = chrono::high_resolution_clock::now();
    	auto duration = chrono::duration_cast<chrono::milliseconds>( tEndGeneral - tStartGeneral ).count();
    	dbAk << "Execution time of the SIMULATION [ms]: " << duration << endl;

    	// Dynamic evaluation of possibly correction of Tc in ModelGraft
    	string replStr = caches[0]->getParentModule()->par("RS");
		if(replStr.compare("ttl_cache") == 0)
		{
			if(!dynamic_tc)
			{
				delete in;
				endSimulation();
			}
			else  // *** DYNAMIC TC ***
			{
				// Compare the SUM of the actual cache sizes of each node
				double Sum_avg_as_cur = 0.0;

    	    	numActiveNodes = 0;


    	    	for (int i=0; i < num_nodes; i++)
    	    	{
    	    		/* In some scenarios, it can happen that some nodes have traffic only at the beginning,
    	    		 * at they do not receive Interest packets anymore due to cache hits on other nodes.
    	    		 * They would be considered as active nodes, but in practice their measured cache size
    	    		 * is nearly '0'. So we try to filter out these nodes.
    	    		 */

    	    		// We sum the measured cache size of all those nodes that have received traffic.
    	    		if(cores[i]->interests != 0)
    	    		{
    	    			dbAk << "** Consistency Check on NODE # " << i << endl;
    	    			numActiveNodes++;
    	    			Sum_avg_as_cur += dynamic_cast<ttl_cache*>(caches[i])->avg_as_curr;

    	    			if(abs(dynamic_cast<ttl_cache*>(caches[i])->target_cache - dynamic_cast<ttl_cache*>(caches[i])->avg_as_curr)/dynamic_cast<ttl_cache*>(caches[i])->target_cache < 0.1)
   	    				{
   	    					dynamic_cast<ttl_cache*>(caches[i])->change_tc = false;   // The Tc value will be not changed.
   	    				}
    	    		}
    	    		else	// Inactive node
    	    			dynamic_cast<ttl_cache*>(caches[i])->change_tc = false;   // Do not collect the measured cache and
    	    																	  // do not change Tc.
    	    	}

    	    	// Only active nodes count
    	    	double Sum_target_cache = dynamic_cast<ttl_cache*>(caches[0])->target_cache * numActiveNodes;

     	    	// Log the Memory demand for the single engine.
    	    	struct rusage rusage;
     	    	getrusage( RUSAGE_SELF, &rusage );
     	    	dbAk << "*** MEMORY:\t" << (size_t)(rusage.ru_maxrss * 1024L) << "  ***\n";


    	    	dbAk << "CYCLE " << sim_cycles << " -\tNUM of ACTIVE NODES among the PARTIAL_N NODES: " << numActiveNodes << endl;

    	    	// *** AKAROA *** Send the measured cache size to the Master through the AkObservationMG() function.
    	    	/*   Param:
    	    	 *   - paramNum: in this case paramNum = 1 since we are sending the measured cache size.
    	    	 *   - Sum_avg_as_cur: sum of the measure cache size from all the active nodes.
    	    	 *   - Sum_target_cache: sum of the target cache (usually  = 10) over all the active nodes.
    	    	 *   - num_nodes: total number of nodes.
    	    	 *   - engineWindow: size of the window of samples for each node.
    	    	 *   - partial_n: actual number of nodes for which the check will be done (usually < numNodes).
    	    	 *   - pid = process id of the current engine.
    	    	 *   - num_threads = number of parallel threads which are executing the current simulation.
    	    	 */

    	    	pid = getpid();
    	    	paramNum = 1;

    	    	AkObservationMG(1, Sum_avg_as_cur, Sum_target_cache, num_nodes, (int)engineWindow, partial_n, pid, num_threads);

    	    	// Waiting for signal to be received from Master
    	    	while(!signal_received)
    	    	{
    	    		sleep(1);
    	    	}

    	    	signal_received = false;	// Resetting variable in order to wait for the next signal
    	    	sleep(1);

    	    	// The simulation is stopped either the 'STOP' signal has been received from the Master, or
    	    	// there have been already 20 MC-TTL cycles (meaning that something wrong is going on).
    	    	if(stop_simulation || sim_cycles > 20)
    	    	{
    	    		dbAk << " *** SIMULATION ENDED AT CYCLE:\t" << sim_cycles << endl;
    	    		delete in;

    	    		// *** Print Cache Measurements ***
    	    		dbAk << " *** CACHE MEASUREMENTS at CYCLE # " << sim_cycles << endl;
    	    		for (int i=0; i < num_nodes; i++)
    	    		{
    	    			dbAk << endl;
    	    			dbAk << "--- NODE # " << i << " ---" << endl;
    	    			dbAk << " - Cache ONLINE AVG measured size: " << caches[i]->get_avg_size() << endl;
    	    			dbAk << " - Cache MAX size: " << caches[i]->get_max_size() << endl;
    	    			dbAk << " - Cache Tc: " << caches[i]->get_tc() << endl;
    	    			if(caches[i]->check_if_two_ttl())
    	    			{
    	    				dbAk << "--- Name Cache ---" << endl;
    	    				dbAk << " - Name Cache ONLINE AVG measured size: " << caches[i]->get_nameCache_avg_size() << endl;
    	    				dbAk << " - Name Cache Tc: " << caches[i]->get_tc_name() << endl;
    	    			}
    	    		}
    	    		// **************************
    	    		// END SIMULATION
    	    		endSimulation();
    	    	}
    			else		// Continue the simulation with another MC-TTL cycle.
    			{
    				dbAk << " *** CONDITION NOT VERIFIED! CONTINUE SIMULATION ***" << endl;
    				sim_cycles++;

    				for (int i=0; i < num_nodes; i++)
    				{
    					dbAk << "NODE:\t" << i << "\n\tOLD Tc = " << caches[i]->tc_node << "\n";
    					dynamic_cast<ttl_cache*>(caches[i])->extend_sim();			// Set the new Tc
    					dbAk << "\tNEW Tc = " << caches[i]->tc_node << "\n";
    					dbAk << "\tOnline Avg Cache Size = " << dynamic_cast<ttl_cache*>(caches[i])->avg_as_curr << "\n";
    				}


    				string decision_policy = caches[0]->par("DS");

    				if (decision_policy.compare("two_ttl")==0)   // Change the Tc of the Name Cache accordingly
    				{
    					for (int i=0; i < num_nodes; i++)
    					{
    						Two_TTL* tTTLPointer = dynamic_cast<Two_TTL *> (caches[i]->get_decisor());
    						tTTLPointer->extend_sim();
    					}
    				}

   					// Clear statistics of the current MC-TTL cycle
   					clear_stat();

   					// Reset sample_counter
   					for(int i=0; i<num_nodes; i++)
   					{
   						sample_counter[i] = 0;
   					}

   					// Reset the HitMiss vector
   					for(int i=0; i<sizeVect; i++)
   						HitMiss[i] = -1;

   					scheduleAt(simTime() + ts, centralized_stable_check);  // Schedule another centralized stability check.
   					dbAk << "***** Re-SCHEDULE Centralized STABLE CHECK PID:\t" << pid << endl;
   				}
   	    	}
		}
		else		// NOT TTL-based scenario. Error message since Parallelization is possible only with TTL caches
		{
			std::stringstream ermsg;
			ermsg<<"Parallelization is possible only with TTL caches! Please correct...";
			severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
		}
    }
}


/*
 * 	Collect only valid HitMiss samples each Ts seconds
 *
 * 	Parameters:
 * 		- n: node ID
 */
void statistics::centralized_stable(int n)
{
	// We check if engineWindow samples have already been collected for node n.
	if(sample_counter[n] != engineWindow)
	{
		//	Only hit rate matters.
		if (caches[n]->hit != 0 )
		{
			// Check if something is changed w.r.t. the previous sample
			if((caches[n]->decision_yes + caches[n]->hit) != events[n])
			{
				// Collect Hit and Miss samples for node 'n' (nodeID 'n' will determine the position in the whole vector
				HitMiss[(n*(int)engineWindow)+sample_counter[n]] = caches[n]->hit;									// HIT
				HitMiss[(num_nodes*(int)engineWindow)+(n*(int)engineWindow)+sample_counter[n]] = caches[n]->miss;	// MISS

				// Update the number of collected samples for node n
				sample_counter[n]++;

				// Update number of events
				events[n] = caches[n]->decision_yes + caches[n]->hit;
			}
			//else	// Sample not collected (i.e., the respective values stay to -1)
		}
		else
		// It can be either a node with #Miss=0 (i.e., inactive), or a node which experienced only miss events so far;
		// in the latter case, we do not collect the sample since we start from the first hit event.
		{
			if(caches[n]->miss == 0)      	// Inactive node  (we should state it as stable anyway)
			{
				HitMiss[(n*(int)engineWindow)+sample_counter[n]] = 0;								// HIT
				HitMiss[(num_nodes*(int)engineWindow)+(n*(int)engineWindow)+sample_counter[n]] = 0;	// MISS

				// Update the number of collected samples for node n
				sample_counter[n]++;
			}
			else	// Node which has experienced only miss events so far (do not collect sample)
			{
				// Update number of events
				events[n] = caches[n]->decision_yes + caches[n]->hit;
			}
		}
	}
}

// Print statistics.
void statistics::finish()
{
	char name[30];

    uint32_t global_hit = 0;
    uint32_t global_miss = 0;
    uint32_t global_interests = 0;
    uint32_t global_data      = 0;
    double global_hit_ratio = 0;

    uint32_t global_repo_load = 0;
	long total_cost = 0;

    double global_avg_distance = 0;
    simtime_t global_avg_time = 0;
    uint32_t global_tot_downloads = 0;

    #ifdef SEVERE_DEBUG
    unsigned int global_interests_sent = 0;
    #endif

    int active_nodes = 0;
    for (int i = 0; i<num_nodes; i++)
	{
    	//TODO: do not always compute cost. Do it only when you want to evaluate the cost in your network
		total_cost += cores[i]->repo_load * cores[i]->get_repo_price();

		// Print measured cache size for each node
		dbAk << "** CACHE-NODE # " << i << ":\tCACHE = " << dynamic_cast<ttl_cache*>(caches[i])->avg_as_curr << endl;

		if (cores[i]->interests)	// Check if the considered node has received Interest packets.
		{
			active_nodes++;
			dbAk << "** NODE # " << i << ":\tHITS = " << caches[i]->hit << "\tMISS = " << caches[i]->miss << endl;
			global_hit  += caches[i]->hit;
			global_miss += caches[i]->miss;
			global_data += cores[i]->data;
			global_interests += cores[i]->interests;
			global_repo_load += cores[i]->repo_load;
			global_hit_ratio += caches[i]->hit * 1./(caches[i]->hit+caches[i]->miss);

			#ifdef SEVERE_DEBUG
				if (	caches[i]->decision_yes + caches[i]->decision_no +
						(unsigned) cores[i]->unsolicited_data
						!=  (unsigned) cores[i]->data + cores[i]->repo_load
				){
					std::stringstream ermsg;
					ermsg<<"caches["<<i<<"]->decision_yes="<<caches[i]->decision_yes<<
						"; caches[i]->decision_no="<<caches[i]->decision_no<<
						"; cores[i]->data="<<cores[i]->data<<
						"; cores[i]->repo_load="<<cores[i]->repo_load<<
						"; cores[i]->unsolicited_data="<<cores[i]->unsolicited_data<<
						". The sum of "<< "decision_yes and decision_no must be data";
					severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
				}
			#endif
		}
    }

    dbAk << "Num Active Nodes: " << active_nodes << endl;
    dbAk << "Num Total Nodes: " << num_nodes << endl;

    // Print and store global statistics

    // The global_hit is the mean hit rate among all the caches.
    sprintf (name, "p_hit");
    recordScalar(name,global_hit_ratio * 1./active_nodes);
    dbAk<<"p_hit/cache: "<<global_hit_ratio * 1./active_nodes<<endl;

    // Mean number of received Interest packets per node.
    sprintf ( name, "interests");
    recordScalar(name,global_interests * 1./num_nodes);

    // Mean number of received Data packets per node.
    sprintf ( name, "data" );
    recordScalar(name,global_data * 1./num_nodes);

    vector<double> global_scheduledReq;
    vector<double> global_validatedReq;
    ShotNoiseContentDistribution* snmPointer;

    // Statistics for popularity classes (only for Shot Noise model)
    cModule* pSubModule = getParentModule()->getSubmodule("content_distribution");
    if (pSubModule)
    {
    	snmPointer = dynamic_cast<ShotNoiseContentDistribution*>(pSubModule);
    	if (snmPointer)
    	{
    		global_scheduledReq.resize(snmPointer->numOfClasses,0);
    		global_validatedReq.resize(snmPointer->numOfClasses,0);
    	}
    }


    for (int i = 0;i<num_clients;i++)
    {
		global_avg_distance += clients[i]->get_avg_distance();
		global_tot_downloads += clients[i]->get_tot_downloads();
		global_avg_time  += clients[i]->get_avg_time();
		//<aa>
		#ifdef SEVERE_DEBUG
		global_interests_sent += clients[i]->get_interests_sent();
		#endif
		//</aa>


		if (snmPointer)
		{
			for(int j=0; j<snmPointer->numOfClasses; j++)
			{
				global_scheduledReq[j] += clients[i]->getScheduledReq(j);
				global_validatedReq[j] += clients[i]->getValidatedReq(j);
			}
		}
	}

    // Mean hit distance.
    sprintf ( name, "hdistance");
    recordScalar(name,global_avg_distance * 1./num_clients);
    dbAk<<"Distance/client: "<<global_avg_distance * 1./num_clients<<endl;

    // Mean download time.
    sprintf ( name, "avg_time");
    recordScalar(name,global_avg_time * 1./num_clients);
    dbAk<<"Time/client: "<<global_avg_time * 1./num_clients<<endl;

    // ** Request Count **
    /*int reqCont = 0;
    for (int j = 0; j<clients[0]->reqCount.size(); j++)
    {
		for (int i = 0;i<num_clients;i++)
		{
			reqCont += clients[i]->reqCount[j];
		}
		dbAk << "Content # " << j+1 << " Requests:\t" << reqCont << endl;
		reqCont = 0;
    }*/

    // SNM statistics
    if(snmPointer)
    {
    	for(int j=0; j<snmPointer->numOfClasses; j++)
    	{
    		sprintf(name, "Scheduled_requests_Class_%d", j+1);		// Absolute number of scheduled requests for that class.
    		recordScalar(name, global_scheduledReq[j]);

    		sprintf(name, "Scheduled_requests_perc_Class_%d", j+1);		// Percentage of scheduled requests.
    		recordScalar(name, global_scheduledReq[j] * 1./snmPointer->totalRequests);

    		sprintf(name, "Validated_requests_Class_%d", j+1);		// Absolute number of validated requests for that class.
    		recordScalar(name, global_validatedReq[j]);

    		sprintf(name, "Validated_requests_relative_perc_Class_%d", j+1);		// Relative percentage of validated requests.
    		recordScalar(name, global_validatedReq[j] * 1./global_scheduledReq[j]);

       		sprintf(name, "Validated_requests_abslute_perc_Class_%d", j+1);		// Absolute percentage of validated requests.
        	recordScalar(name, global_validatedReq[j] * 1./snmPointer->totalRequests);

    		sprintf(name, "Suppressed_requests_Class_%d", j+1);
    		recordScalar(name, global_scheduledReq[j]-global_validatedReq[j]);
    	}
    }

	// Total number of completed downloads (sum over all clients).
    sprintf ( name, "downloads");
    recordScalar(name,global_tot_downloads);

    sprintf ( name, "total_cost");
    recordScalar(name,total_cost);
    dbAk<<"total_cost: "<<total_cost<<endl;


    sprintf ( name, "total_replicas");
    recordScalar(name,total_replicas);
    dbAk<<"total_replicas: "<<total_replicas<<endl;

    // It is the fraction of traffic that is satisfied by some cache inside
    // the network, and thus does not exit the network
    sprintf (name, "inner_hit");
    recordScalar(name , (double) (global_tot_downloads - global_repo_load) / global_tot_downloads) ;

    #ifdef SEVERE_DEBUG
	if (global_tot_downloads == 0)
	{
	       	std::stringstream ermsg;
		ermsg<<"global_tot_downloads == 0";
		severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
	}

		sprintf ( name, "interests_sent");
		recordScalar(name,global_interests_sent);
		dbAk<<"interests_sent: "<<global_interests_sent<<endl;

		if (global_interests_sent != global_tot_downloads){
			std::stringstream ermsg;
			ermsg<<"interests_sent="<<global_interests_sent<<"; tot_downloads="<<
				global_tot_downloads<<
				". If **.size==1 in omnetpp.ini and all links have 0 delay, this "<<
				" is an error. Otherwise, it is not";
			debug_message(__FILE__,__LINE__,ermsg.str().c_str() );
		}
	#endif


	delete [] caches;
	delete [] cores;
	delete [] clients;

	// Close debug file used with Akaroa
	dbAk.close();
}

void statistics::clear_stat()
{
	for (int i = 0;i<num_clients;i++)
	if (clients[i]->is_active() )
	    clients[i]->clear_stat();

    for (int i = 0;i<num_nodes;i++)
        cores[i]->clear_stat();

    for (int i = 0;i<num_nodes;i++)
	    caches[i]->clear_stat();
}

void statistics::stability_has_been_reached(){
	char name[30];
	sprintf (name, "stabilization_time");
	recordScalar(name,stabilization_time);
	dbAk<<"stabilization_time: "<< stabilization_time <<endl;

	clear_stat();
}

void statistics::registerIcnChannel(cChannel* icn_channel){
	#ifdef SEVERE_DEBUG
	if ( std::find(icn_channels.begin(), icn_channels.end(), icn_channel)
			!=icn_channels.end()
	){
        std::stringstream ermsg;
		ermsg<<"Trying to add to statistics object an icn channel already added"<<endl;
	    severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
	}
	#endif
	icn_channels.push_back(icn_channel);
}

void statistics::checkStability()
{
	clear_stat();
	tEndFill = chrono::high_resolution_clock::now();
	scheduleEnd = true;
	scheduleAt(simTime() + ts, centralized_stable_check);
}

// *** In the following: FUNCTIONS related to ANALYTICAL MODEL SOLVING and other UTILITIES (like cache filling) ***

double statistics::calculate_phit_neigh(int node_ID, int cont_ID, float **ratePrev, float **Pin, float **Phit, double *tcVect, double alphaExp, double lambdaVal, long catCard, bool* clientVect, vector<vector<map<int,int> > > &neighMat)
{
    double num;
    double lambda_ex_cont_ID;       // Exogenouas rate for content ID.

    double p_hit_tot = 0.0;
    double probNorm = 0.0;

    if (ratePrev[node_ID][cont_ID] != 0)    // Only if there is incoming traffic the hit probability can be greater than 0.
    {
		if(clientVect[node_ID])
		{
			num = (double)(1.0/(double)pow(cont_ID+1,alphaExp));
			//lambda_ex_cont_ID = (double)(num*content_distribution::zipf.get_normalization_constant())*(lambdaVal);
			// ** ACCURACY BOOST - START**
			lambda_ex_cont_ID = (double)(num*content_distribution::zipf[0]->get_normalization_constant())*(lambdaVal);
			// ** ACCURACY BOOST - END**
			probNorm += lambda_ex_cont_ID;      // It will be added to the sum of the incoming miss streams.
		}

		if (neighMat[node_ID][cont_ID].size() > 0)
		{
			int neigh;

			//for(uint32_t i=0; i < neighMat[node_ID][cont_ID].size(); i++)
			for (std::map<int,int>::iterator it = neighMat[node_ID][cont_ID].begin(); it!=neighMat[node_ID][cont_ID].end(); ++it)
			{
				neigh = it->first;
				if(ratePrev[neigh][cont_ID]!=0)
					probNorm += (ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))*(1./it->second); // we take into account ri,j
			}

			if(probNorm == 0)
			{
				p_hit_tot = 0;
				return p_hit_tot;
			}

			//dbAk << "PROB NORM: " << probNorm << endl;


			//for(uint32_t i=0; i < neighMat[node_ID][cont_ID].size(); i++)
			for (std::map<int,int>::iterator it = neighMat[node_ID][cont_ID].begin(); it!=neighMat[node_ID][cont_ID].end(); ++it)
			{
				neigh = it->first;
				if (ratePrev[neigh][cont_ID]!=0)
				{
					double Aij  = 0;
					double partial_sum = 0;
					//for (uint32_t j=0; j < neighMat[node_ID][cont_ID].size(); j++)  // If there are at least 2 neighbors (different from a client)
					for (std::map<int,int>::iterator it2 = neighMat[node_ID][cont_ID].begin(); it2!=neighMat[node_ID][cont_ID].end(); ++it2)
					{
						if (it2!=it)	// The 'j' neigh must be different than the one selected to calculate the conditional prob.
						{
							neigh = it2->first;
							if(ratePrev[neigh][cont_ID]!=0)
								partial_sum += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*tcVect[node_ID])*(1./it2->second);
								//partial_sum += (ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])* max((double)0, tcVect[node_ID]-tcVect[neigh]);
						}
					}
					if (clientVect[node_ID])
					{
						partial_sum += (lambda_ex_cont_ID)*tcVect[node_ID];
					}

					neigh = it->first;

					if(meta_cache == LCE)
					{
						Aij += (((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID]) * max((double)0, tcVect[node_ID]-tcVect[neigh]))*(1./it->second) + partial_sum);
						// *** a-NET style ***
						//Aij += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID]) * tcVect[node_ID] + partial_sum);

						// Sum conditional probabilities.
						p_hit_tot += (1 - exp(-Aij))*((ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))*(1./it->second)/probNorm);
					}
					else if(meta_cache == fixP)
					{
						// "partial_sum" only in the exponent with Tc2-Tc1
						Aij += (1-q)*(1-exp(-ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh])) +
								exp(-ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh]) *
								(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID]) * max(double(0), tcVect[node_ID] - tcVect[neigh]) + partial_sum)));
						// "partial_sum" in each exponent.
						//Aij += (1-q)*(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh] + partial_sum))) +
						//		exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh] + partial_sum)) *
						//		(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID]) * max(double(0), tcVect[node_ID] - tcVect[neigh]) + partial_sum)));


						p_hit_tot += ((q * Aij) / (1 - (1-q)*Aij))*((ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))/probNorm);
					}
					else
					{
						dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
						exit(0);
					}
				}
			}
		}
		if (clientVect[node_ID])  // we have to calculate the conditional probability with respect to the client
		{
			double Aij = 0;
			double partial_sum = 0;
			int neigh;
			//for (uint32_t j=0; j < neighMat[node_ID][cont_ID].size(); j++)
			if (neighMat[node_ID][cont_ID].size() > 0)
			{
				for (std::map<int,int>::iterator it = neighMat[node_ID][cont_ID].begin(); it!=neighMat[node_ID][cont_ID].end(); ++it)
				{
					neigh = it->first;

					if(ratePrev[neigh][cont_ID]!=0)
						partial_sum += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*tcVect[node_ID])*(1./it->second);
						//partial_sum += (ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*max((double)0, tcVect[node_ID]-tcVect[neigh]);
				}
			}

			if(meta_cache == LCE)
			{
				Aij += ((lambda_ex_cont_ID)*tcVect[node_ID] + partial_sum);
				p_hit_tot += (1 - exp(-Aij))*(lambda_ex_cont_ID/probNorm);
			}
			else if (meta_cache == fixP)
			{
				Aij += q * (1 - exp(-(lambda_ex_cont_ID*tcVect[node_ID] + partial_sum)));
				p_hit_tot += (Aij / (exp(-(lambda_ex_cont_ID*tcVect[node_ID] + partial_sum)) + Aij)) * (lambda_ex_cont_ID/probNorm);
			}
			else
			{
				dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
				exit(0);
			}
		}
    }

	return p_hit_tot;
}



double statistics::MeanSquareDistance(uint32_t catCard, double **prevRate, double **currRate, int colIndex)
{
	double sumCurr = 0;
	double sumPrev = 0;
	//double sumVal = 0;
	double msdVal;
	for (uint32_t m=0; m < catCard; m++)
	{
		//sumVal += pow((currRate[colIndex][m] - prevRate[colIndex][m]),2);
		sumCurr += currRate[colIndex][m];
		sumPrev += prevRate[colIndex][m];
	}

	//msdVal = sumVal/(double)catCard;
	msdVal = abs(sumCurr-sumPrev);

	return msdVal;
}


void statistics::cacheFillNaive()
{
	dbAk << "Filling Caches with Naive method...\n" << endl;

	int N = num_nodes;									   			// Number of nodes.
	//uint32_t M = content_distribution::zipf.get_catalog_card();		// Number of contents.
	// ** ACCURACY BOOST - START**
	uint32_t M = content_distribution::zipf[0]->get_catalog_card();		// Number of contents.
	// ** ACCURACY BOOST - END**
	uint32_t cSize_targ = (double)caches[0]->get_size();

	//double alphaVal = content_distribution::zipf.get_alpha();
	//double normConstant = content_distribution::zipf.get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - START**
	double alphaVal = content_distribution::zipf[0]->get_alpha();
	double normConstant = content_distribution::zipf[0]->get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - END**

	double *pZipf = new double[M];
	double num;

	uint32_t cont_id, name;

	double q = 1.0;

	DecisionPolicy* decisor;
	Fix* dp1;

	decisor = caches[0]->get_decisor();
	dp1 = dynamic_cast<Fix*> (decisor);;

	if(dp1)
	{
		q = dp1->p;
	}


	ccn_data* data = new ccn_data("data",CCN_D);

	for(int n=0; n < N; n++)
	{
		decisor = caches[n]->get_decisor();
		dp1 = dynamic_cast<Fix*> (decisor);;

		if(dp1)
		{
			dp1->setProb(1.0);
		}

		for (uint32_t m=0; m < M; m++)
		{
			num = (1.0/pow(m+1,alphaVal));
			pZipf[m] = (double)(num*normConstant);
		}

		std::random_device rd;     // only used once to initialise (seed) engine
		std::mt19937 rng(rd());    // random-number engine used (Mersenne-Twister in this case)
		std::uniform_int_distribution<uint32_t> uni(1,M); // guaranteed unbiased

		for (uint32_t k=0; k < cSize_targ; k++)
		{
			bool found = false;
			while(!found)
			{
				//name = std::floor((rand() / ((double)RAND_MAX+1))*(M + 1));
			    name = uni(rng);
				//if (pZipf[name] > dblrand())
				if (pZipf[name] != 0)
				{
					pZipf[name] = 0;
					found = true;
				}
			}

			cont_id = name;
			//cont_id = k+1;
			chunk_t chunk = 0;
			__sid(chunk, cont_id);
			__schunk(chunk, 0);
			data -> setChunk (chunk);
			data -> setHops(0);
			data->setTimestamp(simTime());
			caches[n]->store(data);
		}
		dbAk << "Cache Node # " << n << " :\n";
		caches[n]->dump();
		if(dp1)
		{
			dp1->setProb(q);
		}

	}
	dbAk << "MEAN hit ratio after CACHE hot start naive: unknown\n" << endl;
	delete data;
}


// *** CACHE MODEL SCALABLE ***
void statistics::cacheFillModel_Scalable_Approx(const char* phase)
{
	chrono::high_resolution_clock::time_point tStartAfterFailure;
	chrono::high_resolution_clock::time_point tEndAfterFailure;
	if(strcmp(phase,"init")!=0)
		tStartAfterFailure = chrono::high_resolution_clock::now();
	//long M = (long)content_distribution::zipf.get_catalog_card();		// Catalog cardinality.
	// ** ACCURACY BOOST - START**
	long M = (long)content_distribution::zipf[0]->get_catalog_card();		// Catalog cardinality.
	// ** ACCURACY BOOST - END**
	int N = num_nodes;												// Number of nodes.
	double Lambda = clients[0]->getLambda();						// Aggregate lambda of exogenous requests.
	uint32_t cSize_targ = (double)caches[0]->get_size();			// Target Cache size.

	dbAk << "Model CACHE SIZE = " << cSize_targ << endl;

	//double alphaVal = content_distribution::zipf.get_alpha();
	//double normConstant = content_distribution::zipf.get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - START**
	double alphaVal = content_distribution::zipf[0]->get_alpha();
	double normConstant = content_distribution::zipf[0]->get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - END**

	string forwStr = caches[0]->getParentModule()->par("FS");
	dbAk << "*** Model Forwarding Strategy : " << forwStr << " ***" << endl;

	// Retrieve meta-caching algorithm
	DecisionPolicy* decisor = caches[0]->get_decisor();
	Always* dp1 = dynamic_cast<Always*> (decisor);
	if(dp1)
	{
		meta_cache = LCE;
		dpString = "LCE";
		dbAk << "*** LCE *** Decision Policy!" << endl;
	}
	else
	{
		Fix* dp2 = dynamic_cast<Fix*> (decisor);
		if(dp2)
		{
			meta_cache = fixP;
			q = dp2->p;
			dpString = "fixP";
			dbAk << "*** fixP *** Decision Policy with q = " << q << endl;
		}
		else
		{
			dbAk << "Decision policy not supported!" << endl;
			exit(0);
		}
	}

	dbAk << "***** CACHE FILLING WITH MODEL *****" << endl;

	// Definition and initialization of useful data structures.
	float **prev_rate;		// Request rate for each content at each node at the 'Previous Step'.
	float **curr_rate;		// Request rate for each content at each node at the 'Current Step'.
	float **p_in;			// Pin probability for each content at each node.
	float **p_hit;			// Phit probability for each content at each node.

	// All the previous structures will be matrices of size [M][N].
	prev_rate = new float*[N];
	curr_rate = new float*[N];
	p_in = new float*[N];
	p_hit = new float*[N];

	for (int i=0; i < N; i++)
	{
		prev_rate[i] = new float[M];
		curr_rate[i] = new float[M];
		p_in[i] = new float[M];
		p_hit[i] = new float[M];
	}

	double *tc_vect = new double[N]; 		// Vector containing the 'characteristic times' of each node.


	/*double **p_in_temp;				// Temp vector to find the max p_in per each node at the end of each iteration.
	p_in_temp = new double*[N];
	for(int n=0; n < N; n++)
			p_in_temp[n] = new double[M];
	 */

	double *pHitNode = new double[N];
	fill_n(pHitNode,N,0.0);
	double prev_pHitTot = 0.0;
	double curr_pHitTot = 0.0;


	double *sumCurrRate = new double[N];	// It will contain the total incoming rate for each node.
	fill_n(sumCurrRate,N,0.0);

	//vector<int> inactiveNodes;
	vector<int> activeNodes;

	// *** DISABLED FOR PERF EVALUATION ***
	int **steadyCache; 			// Matrix containing the IDs of the content the will be cached inside the active nodes for the hot start.
	if(!onlyModel)
	{
		steadyCache = new int*[N];
		for (int n=0; n < N; n++)
		{
			steadyCache[n] = new int[cSize_targ];
			fill_n(steadyCache[n], cSize_targ, M+1);
		}
	}

	// *** INITIALIZATION ***
	// At step '0', the incoming rates at each node are supposed to be the same,
	// i.e., prev_rate(i,n) = zipf(i) * Lambda.
	// As a consequence, the characteristic times of all the nodes will be initially the same, and so the Pin.

	int step = 0;
	double num = 0;
	bool climax;

	dbAk << "Iteration # " << step << " - INITIALIZATION" << endl;

	for (long m=0; m < M; m++)
	{
		num = (1.0/pow(m+1,alphaVal));
		prev_rate[0][m] = (float)(num*normConstant)*Lambda;
		sumCurrRate[0] += prev_rate[0][m];
	}

	for (int n=1; n < N; n++)
	{
		std::copy(&prev_rate[0][0], &prev_rate[0][M], prev_rate[n]);
		sumCurrRate[n] = sumCurrRate[0];
	}


	// The Tc will be initially the same for all the nodes, so we pass just the first column of the prev_rate.
	// In the following steps it will be Tc_val(n) = compute_Tc(...,prev_rate, n-1).

	double tc_val = compute_Tc_single_Approx(cSize_targ, alphaVal, M, prev_rate, 0, dpString, q);

	dbAk << "Computed Tc during initialization:\t" << tc_val << endl;

	if(meta_cache == LCE)
	{
		for (int n=0; n < N; n++)
		{
			tc_vect[n] = tc_val;
			for (long m=0; m < M; m++)
			{
				p_in[n][m] = 1 - exp(-prev_rate[n][m]*tc_vect[n]);
				p_hit[n][m] = p_in[n][m];
				pHitNode[n] += (prev_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}
			prev_pHitTot += pHitNode[n];
		}
	}

	else if (meta_cache == fixP)
	{
		for (int n=0; n < N; n++)
		{
			tc_vect[n] = tc_val;
			for (long m=0; m < M; m++)
			{
				p_in[n][m] = (q * (1.0 - exp(-prev_rate[n][m]*tc_vect[n])))/(exp(-prev_rate[n][m]*tc_vect[n]) + q * (1.0 - exp(-prev_rate[n][m]*tc_vect[n])));
				p_hit[n][m] = p_in[n][m];
				pHitNode[n] += (prev_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}
			prev_pHitTot += pHitNode[n];
		}
	}
	else
	{
		dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
		exit(0);
	}

	prev_pHitTot /= N;
	dbAk << "pHit Tot Init - " << prev_pHitTot << endl;


	// *** ITERATIVE PROCEDURE ***

	// Cycle over more steps
	int slots = 30;

	//double **MSD; 			// matrix[slots][N]; it will contain computed Mean Square Distance at each step of the iteration.
							// Useful as a stop criterion for the iterative procedure.

	vector<map<int,int> >  neighMatrix;
	neighMatrix.resize(N);

	/*vector<vector<map<int,double> > > rateNeighMatrix;
	rateNeighMatrix.resize(N);
	for(int n=0; n < N; n++)
		rateNeighMatrix[n].resize(M);*/


	bool* clientVector = new bool[N];					// Vector indicating if the current node has an attached client.
	for (int n=0; n < N; n++)
		clientVector[n] = false;


	vector<int> repoVector;


    int l;

    	// Retrieve the Repository storing all the contents

    	repo_t repo = __repo(1);    // The repo is the one associated to the position of the '1' inside the string
    	l = 0;
    	while (repo)
    	{
    		if (repo & 1)
			{
 				//repoVector[m] = content_distribution::repositories[l];  // So far, the replication of seed copies is supposed
    																    // to be 1 (i.e., the seed copy of each content is stored only in one repo)
    			//repoVector[m].push_back(content_distribution::repositories[l]);	// In case of more repos stories the seed copies of a content.
    			repoVector.push_back(content_distribution::repositories[l]);	// In case of more repos stories the seed copies of a content.
 				//if (m<20)
 				//	dbAk << "The Repo of content # " << m << " is: " << repoVector[m] << endl;
    		}
    		repo >>= 1;
    		l++;
    	}

	int outInt;
	int target;


	std::vector<int>::iterator itRepoVect;

	for (int n=0; n < N; n++)  // NODES
	{
	    for(int i=0; i < num_clients; i++)
	    {
	    	if(clients_id[i] == n)
	    	{
	    		clientVector[n] = true;
	    		break;
	    	}
	    }

	    itRepoVect = find (repoVector.begin(), repoVector.end(), n);
		if (itRepoVect != repoVector.end() )
			continue;
		else
		{
			for(itRepoVect = repoVector.begin(); itRepoVect != repoVector.end(); ++itRepoVect)
			{
				outInt = cores[n]->getOutInt(*itRepoVect);
				target = caches[n]->getParentModule()->gate("face$o",outInt)->getNextGate()->getOwnerModule()->getIndex();
				neighMatrix[target].insert( pair<int,int>(n,1));
			}
		}
	}


	// Iterations
	for (int k=0; k < slots; k++)
	{
		step++;
		dbAk << "Iteration # " << step << endl;

		// Calculate the 'current' request rate for each content at each node.
		for (int n=0; n < N; n++)			// NODES
		{
			double sum_curr_rate = 0;
			double sum_prev_rate = 0;

			//dbAk << "NODE # " << n << endl;
			for (long m=0; m < M; m++)		// CONTENTS
			{
				double neigh_rate = 0;			// Cumulative miss rate from neighbors for the considered content.

			    if (neighMatrix[n].size() > 0 || clientVector[n])	// Sum the miss streams of the neighbors for the considered content.
			    {
			    	//for (uint32_t i=0; i < neighMatrix[n][m].size(); i++)
			    	for (std::map<int,int>::iterator it = neighMatrix[n].begin(); it!=neighMatrix[n].end(); ++it)
			    	{
			    		// The miss stream of the selected neighbor will depend on its hit probability for the selected
			    		// content (i.e., Phit(neigh,m). This can be calculated using the conditional probabilities
			    		// Phit(neigh,m|j) = 1 - exp(-A_neigh_j). It expresses the probability that a request for 'm'
			    		// hits cache 'neigh', provided that it comes from cache 'j'. Only neighboring caches for which
			    		// the neigh represents the next hop will be considered.
			    		//int neigh = neighMatrix[n][m][i];
			    		int neigh = it->first;
			    		int numPot = it->second;    // number of potential targets for the neighbor

			    		if(meta_cache == LCE)
			    		{
			    			if(prev_rate[neigh][m]*tc_vect[neigh] <= 0.01)
			    				p_hit[neigh][m] = prev_rate[neigh][m]*tc_vect[neigh];
			    			else
			    				p_hit[neigh][m] = calculate_phit_neigh_scalable(neigh, m, prev_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
			    		}
			    		else if(meta_cache == fixP)
			    		{
			    			if(q*prev_rate[neigh][m]*tc_vect[neigh] <= 0.01)
			    				p_hit[neigh][m] = q*(prev_rate[neigh][m]*tc_vect[neigh]);
			    			else
			    				p_hit[neigh][m] = calculate_phit_neigh_scalable(neigh, m, prev_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
			    		}
			    		else
						{
							dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
							exit(0);
						}

						if (p_hit[neigh][m] > 1)
							dbAk << "Node: " << n << "\tContent: " << m << "\tNeigh: " << neigh << "\tP_hit: " << p_hit[neigh][m] << endl;

						//neigh_rate += (prev_rate[neigh][m]*(1-p_hit[neigh][m]));
						neigh_rate += (prev_rate[neigh][m]*(1-p_hit[neigh][m]))*(1./numPot);

						//rateNeighMatrix[n][m][neigh] = 0; 		// Reset the result of the previous iteration.
						//rateNeighMatrix[n][m][neigh] += (prev_rate[neigh][m]*(1-p_hit[neigh][m]));
			    	}
			    }


			    if (clientVector[n])	// In case a client is attached to the current node.
		    	{
					num = (1.0/(double)pow(m+1,alphaVal));
					neigh_rate += (num*normConstant)*Lambda;
		    	}

			    curr_rate[n][m] = neigh_rate;

			    sum_curr_rate += curr_rate[n][m];
			    sum_prev_rate += prev_rate[n][m];

			    prev_rate[n][m] = curr_rate[n][m];

			}  // contents

			//dbAk << "Iteration # " << step << " Node # " << n << " Incoming Rate: " << sum_curr_rate << endl;

			sumCurrRate[n] = sum_curr_rate;
			climax = false;

			// Calculate the new Tc and Pin for the current node.
			if (sum_curr_rate != 0) 	// The current node is hit either by exogenous traffic or by miss streams from
										// other nodes (or by both)
			{
				dbAk << "NODE # " << n << " Sum Current Rate: " << sumCurrRate[n] << endl;
				tc_vect[n] = compute_Tc_single_Approx(cSize_targ, alphaVal, M, curr_rate, n, dpString, q);

				dbAk << "Node # " << n << " - Tc " << tc_vect[n] << endl;
				if(meta_cache == LCE)
				{
					for(long m=0; m < M; m++)
					{
						if(!climax)
						{
							if(curr_rate[n][m]*tc_vect[n] <= 0.01)
							{
								p_in[n][m] = curr_rate[n][m]*tc_vect[n];
							}
							else
							{
								p_in[n][m] = 1 - exp(-curr_rate[n][m]*tc_vect[n]);
								climax = true;
							}
						}
						else
						{
							p_in[n][m] = 1 - exp(-curr_rate[n][m]*tc_vect[n]);
							if(curr_rate[n][m]*tc_vect[n] <= 0.01)
							{
								for (long z=m+1; z < M; z++)
									p_in[n][z] = curr_rate[n][z]*tc_vect[n];
							}
							break;

						}
					}
					climax = false;
				}
				else if(meta_cache == fixP)
				{
					for(long m=0; m < M; m++)
					{
						if(!climax)
						{

							if(q*curr_rate[n][m]*tc_vect[n] <= 0.01)
							{
								p_in[n][m] = q * curr_rate[n][m]*tc_vect[n];
							}
							else
							{
								p_in[n][m] = (q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])))/(exp(-curr_rate[n][m]*tc_vect[n]) + q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])));
								climax = true;
							}
						}
						else
						{
							p_in[n][m] = (q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])))/(exp(-curr_rate[n][m]*tc_vect[n]) + q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])));
							if(q*curr_rate[n][m]*tc_vect[n] <= 0.01)
							{
								for (long z=m+1; z < M; z++)
									p_in[n][z] = q * curr_rate[n][z]*tc_vect[n];
							}
							break;
						}
					}
					climax = false;
				}
				else
				{
					dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
					exit(0);
				}

				// Calculate the Phit of the node hosting one of the repos (it can happen that it does not forward any
				// traffic, thus it is not a neighbor for any node, and so the logic of the p_hit calculus followed
				// before does not apply.
				if (find (content_distribution::repositories, content_distribution::repositories + num_repos, n)
							!= content_distribution::repositories + num_repos)
				{
					for(long m=0; m < M; m++)
					{
						if(meta_cache == LCE)
						{
							if(curr_rate[n][m]*tc_vect[n] <= 0.01)
								p_hit[n][m] = curr_rate[n][m]*tc_vect[n];
							else
								p_hit[n][m] = calculate_phit_neigh_scalable(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else if(meta_cache == fixP)
						{
							if(q*curr_rate[n][m]*tc_vect[n] <= 0.01)
								p_hit[n][m] = q*(curr_rate[n][m]*tc_vect[n]);
							else
								p_hit[n][m] = calculate_phit_neigh_scalable(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else
						{
							dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
							exit(0);
						}
					}
				}
			}
			else		// It means that the current node will not be hit by any traffic.
						// So we zero the miss stream coming from this node by putting p_in=1 for each content.
			{
				tc_vect[n] = numeric_limits<double>::max();   // Like infinite value;
				//dbAk << "Iteration # " << step << " NODE # " << n << " Tc - " << tc_vect[n] << endl;
				for(long m=0; m < M; m++)
				{
					p_in[n][m] = 0;
					p_hit[n][m] = 0;
				}

			}

		} // nodes

		curr_pHitTot = 0;
		for(int n=0; n < N; n++)
		{
			pHitNode[n] = 0;
			if(sumCurrRate[n]!=0)
			{
				for (long m=0; m < M; m++)
				{
					pHitNode[n] += (curr_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
				}
				curr_pHitTot += pHitNode[n];
			}

		}

		// *** EXIT CONDITION ***
		if( (abs(curr_pHitTot - prev_pHitTot)/curr_pHitTot) < 0.005 )
			break;
		else		// For the NRR implementation, the actual cache fill is moved here in order to update the neighMatrix
					// computation at the next step
		{
			prev_pHitTot = curr_pHitTot;

			/// ***** TRY *****
			for(int n=0; n < N; n++)
			{
				if(sumCurrRate[n]!=0)
				{
					for (long m=0; m < M; m++)
					{
						curr_rate[n][m] = 0;
					}
				}
			}
		}
	}  // Successive step

	int maxPin;

	double pHitTotMean = 0;
	double pHitNodeMean = 0;

	activeNodes.clear();

	for(int n=0; n < N; n++)
	{
		// Allocate contents only for nodes interested by traffic (it depends on the frw strategy)
		if(tc_vect[n] != numeric_limits<double>::max())
		{
			activeNodes.push_back(n);

			// Calculate di p_hit mean of the node
			for(uint32_t m=0; m < M; m++)
			{
				pHitNodeMean += (curr_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}

			// *** DISABLED for perf measurements
			if(!onlyModel)
			{
				// *** DETERMINING CONTENTS TO BE PUT INSIDE CACHES***
				// Choose the contents to be inserted into the cache.
				for(uint32_t k=0; k < cSize_targ; k++)
				{
					maxPin = distance(p_in[n], max_element(p_in[n], p_in[n] + M));  // Position of the highest popular object (i.e., its ID).
					// *** NRR
					if(p_in[n][maxPin] == 0.0)			// There are few contents than cSize_targ that can be inside the cache
						break;
					steadyCache[n][k] = maxPin;

					//dbAk << "NODE # " << n << " Content # " << maxPin << endl;

					//p_in_temp[n][k] = p_in[n][maxPin];
					//p_hit_temp[n][k] = p_hit[n][maxPin];
					p_in[n][maxPin] = 0;
				}
			}


			pHitTotMean += pHitNodeMean;
			dbAk << "NODE # " << n << endl;
			dbAk << "-> Mean Phit: " << pHitNodeMean << endl;
//			dbAk << "-> Steady Cache: Content - Pin - Phit\n";
//			for (uint32_t k=0; k < cSize_targ; k++)
//				dbAk << steadyCache[n][k] << " - " << p_in_temp[n][k] << " - " << p_hit_temp[n][k] << endl;
			dbAk << endl;
			pHitNodeMean = 0;
		}
	}

	if(strcmp(phase,"init")==0)
		dbAk << "Number of Active Nodes 1: " << activeNodes.size() << endl;
	else
		dbAk << "Number of Active Nodes 2: " << num_nodes - activeNodes.size() << " Time: " << simTime() << endl;

	pHitTotMean = pHitTotMean/(activeNodes.size());
	//pHitTotMean = pHitTotMean/(N);

	if(strcmp(phase,"init")==0)
		dbAk << "MODEL - P_HIT MEAN TOTAL AFTER CACHE FILL: " << pHitTotMean << endl;
	else
		dbAk << "MODEL - P_HIT MEAN TOTAL AFTER ROUTE re-CALCULATION: " << pHitTotMean << endl;

	dbAk << "*** Tc of nodes ***\n";
	for(int n=0; n < N; n++)
	{
		dbAk << "Tc-" << n << " --> " << tc_vect[n] << endl;
	}

    // *** DISABLED per perf evaluation
	if(!onlyModel)
	{
		// *** REAL CACHE FILLING ***
		if(strcmp(phase,"init")==0)
		{
			dbAk << "Filling Caches with Model results...\n" << endl;
			int node_id;
			uint32_t cont_id;

			// In case of FIX probabilistic with need to temporarly change the caching prob id order to fill the cache
			DecisionPolicy* decisor;
			Fix* dp1;

			ccn_data* data = new ccn_data("data",CCN_D);

			for(unsigned int n=0; n < activeNodes.size(); n++)
			{
				node_id = activeNodes[n];

				decisor = caches[node_id]->get_decisor();
				dp1 = dynamic_cast<Fix*> (decisor);;

				if(dp1)
				{
					dp1->setProb(1.0);
				}

				// Empty the cache from the previous step.
				caches[node_id]->flush();

				//for (int k=cSize_targ-1; k >= 0; k--)   // Start inserting contents with the smallest p_in.
				for (unsigned int k=0; k < cSize_targ; k++)
				{
					cont_id = (uint32_t)steadyCache[node_id][k]+1;
					if (cont_id == M+2)			// There are few contents than cSize_targ that can be inside the cache
						break;
					chunk_t chunk = 0;
					__sid(chunk, cont_id);
					__schunk(chunk, 0);
					//ccn_data* data = new ccn_data("data",CCN_D);
					data -> setChunk (chunk);
					data -> setHops(0);
					data->setTimestamp(simTime());
					caches[node_id]->store(data);
				}
				//dbAk << "Cache Node # " << node_id << " :\n";
				//caches[node_id]->dump();

				if(dp1)
				{
					dp1->setProb(q);
				}

			}

			delete data;
		}
	}



	// DISABLED for perf eval
	// *** NEW LINK-LOAD EVALUATION WITH MEAN AND DISTRIBUTION ***

	/*double linkTraffic = 0;
	int neigh;
	vector<map<int,double> > totalNeighRate;
	totalNeighRate.resize(N);
	dbAk << "*** Link State ***" << endl;
	dbAk << "Extreme A -- Link Load [Bw=1Mbps] -- Extreme B" << endl;
	dbAk << endl;*/

	/*for (int n=0; n < N; n++)
	{
		for (int m=0; m < M; m++)
		{
			map<int,double>::iterator it = rateNeighMatrix[n][m].begin();
			while(it!=rateNeighMatrix[n][m].end())
			{
				neigh = it->first;
				map<int,double>::iterator itInner = totalNeighRate[n].find(neigh);
				if(itInner == totalNeighRate[n].end()) // Insert for the first time
					totalNeighRate[n][neigh] = it->second;
				else										// Neigh already inserted before.
					totalNeighRate[n][neigh] += it->second;
				++it;
			}
		}
	}*/

	/*for (int n=0; n < N; n++)
	{
		for (int m=0; m < M; m++)
		{
			map<int,double>::iterator it = rateNeighMatrix[n][m].begin();
			while(it!=rateNeighMatrix[n][m].end())
			{
				neigh = it->first;

				// *** DISTRIBUTION ***
				linkTraffic = it->second;
				map<int,double>::iterator itReverse = rateNeighMatrix[neigh][m].find(n);
				if(itReverse != rateNeighMatrix[neigh][m].end()) // There is traffic for that content also in the opposite direction
					linkTraffic += itReverse->second;
				if (n==0 && neigh==1)
				{
					dbAk << "TIER 1 - Content # " << m+1 << "\tLOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
				}
				else if (n==1 && neigh==3)
				{
					dbAk << "TIER 2 - Content # " << m+1 << "\tLOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
				}
				else if (n==3 && neigh==7)
				{
					dbAk << "TIER 3 - Content # " << m+1 << "\tLOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
				}
				linkTraffic = 0;
				// ***********************

				map<int,double>::iterator itInner = totalNeighRate[n].find(neigh);
				if(itInner == totalNeighRate[n].end()) // Insert for the first time
					totalNeighRate[n][neigh] = it->second;
				else								  // Neigh already inserted before.
					totalNeighRate[n][neigh] += it->second;
				++it;
			}
		}
	}

	for (int n=0; n < N; n++)
	{
		for(map<int,double>::iterator it=totalNeighRate[n].begin(); it!=totalNeighRate[n].end(); ++it)
		{
			neigh = it->first;
			linkTraffic += it->second;
			map<int,double>::iterator itInner = totalNeighRate[neigh].find(n);  // Sum the traffic in the opposite direction
			if(itInner != totalNeighRate[neigh].end())  // There is also traffic in the opposite direction.
			{
				linkTraffic += itInner->second;
				totalNeighRate[neigh].erase(itInner);
			}
			//dbAk << "Node " << n << "\t- " << (double)((linkTraffic*(1536*8))/1000000) << " -\tNode " << neigh << endl;
			if (n==0 && neigh==1)
			{
				dbAk << "TIER 1 - " << "\tMEAN LOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
			}
			else if (n==1 && neigh==3)
			{
				dbAk << "TIER 2 - " << "\tMEAN LOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
			}
			else if (n==3 && neigh==7)
			{
				dbAk << "TIER 3 - " << "\tMEAN LOAD - " << (double)((linkTraffic*(1536*8))/1000000) << endl;
			}
			linkTraffic = 0;
		}
	}*/

	// *******************************

	if(strcmp(phase,"init")!=0)
	{
		tEndAfterFailure = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>( tEndAfterFailure - tStartAfterFailure  ).count();
		dbAk << "Execution time of the model after failure [ms]: " << duration << endl;
	}
	// De-allocating memory
	for (int n = 0; n < N; ++n)
	{
		delete [] prev_rate[n];
		delete [] curr_rate[n];
		delete [] p_in[n];
		delete [] p_hit[n];
	}

	delete [] prev_rate;
	delete [] curr_rate;
	delete [] p_in;
	delete [] p_hit;

}



double statistics::calculate_phit_neigh_scalable(int node_ID, int cont_ID, float **ratePrev, float **Pin, float **Phit, double *tcVect, double alphaExp, double lambdaVal, long catCard, bool* clientVect, vector<map<int,int> > &neighMat)
{
    double num;
    double lambda_ex_cont_ID;       // Exogenouas rate for content ID.

    double p_hit_tot = 0.0;
    double probNorm = 0.0;

    if (ratePrev[node_ID][cont_ID] != 0)    // Only if there is incoming traffic the hit probability can be greater than 0.
    {
		if(clientVect[node_ID])
		{
			num = (double)(1.0/(double)pow(cont_ID+1,alphaExp));
			//lambda_ex_cont_ID = (double)(num*content_distribution::zipf.get_normalization_constant())*(lambdaVal);
			// ** ACCURACY BOOST - START**
			lambda_ex_cont_ID = (double)(num*content_distribution::zipf[0]->get_normalization_constant())*(lambdaVal);
			// ** ACCURACY BOOST - END**
			probNorm += lambda_ex_cont_ID;      // It will be added to the sum of the incoming miss streams.
		}

		if (neighMat[node_ID].size() > 0)
		{
			int neigh;

			//for(uint32_t i=0; i < neighMat[node_ID][cont_ID].size(); i++)
			for (std::map<int,int>::iterator it = neighMat[node_ID].begin(); it!=neighMat[node_ID].end(); ++it)
			{
				neigh = it->first;
				if(ratePrev[neigh][cont_ID]!=0)
					probNorm += (ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))*(1./it->second); // we take into account ri,j
			}

			if(probNorm == 0)
			{
				p_hit_tot = 0;
				return p_hit_tot;
			}

			//dbAk << "PROB NORM: " << probNorm << endl;


			//for(uint32_t i=0; i < neighMat[node_ID][cont_ID].size(); i++)
			for (std::map<int,int>::iterator it = neighMat[node_ID].begin(); it!=neighMat[node_ID].end(); ++it)
			{
				neigh = it->first;
				if (ratePrev[neigh][cont_ID]!=0)
				{
					double Aij  = 0;
					double partial_sum = 0;
					//for (uint32_t j=0; j < neighMat[node_ID][cont_ID].size(); j++)  // If there are at least 2 neighbors (different from a client)
					for (std::map<int,int>::iterator it2 = neighMat[node_ID].begin(); it2!=neighMat[node_ID].end(); ++it2)
					{
						if (it2!=it)	// The 'j' neigh must be different than the one selected to calculate the conditional prob.
						{
							neigh = it2->first;
							if(ratePrev[neigh][cont_ID]!=0)
								partial_sum += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*tcVect[node_ID])*(1./it2->second);
								//partial_sum += (ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])* max((double)0, tcVect[node_ID]-tcVect[neigh]);
						}
					}
					if (clientVect[node_ID])
					{
						partial_sum += (lambda_ex_cont_ID)*tcVect[node_ID];
					}

					neigh = it->first;

					if(meta_cache == LCE)
					{
						Aij += (((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID]) * max((double)0, tcVect[node_ID]-tcVect[neigh]))*(1./it->second) + partial_sum);
						// *** a-NET style ***
						//Aij += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID]) * tcVect[node_ID] + partial_sum);

						// Sum conditional probabilities.
						//if(Aij < 0.01)
						//	p_hit_tot += (-Aij)*((ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))*(1./it->second)/probNorm);
						//else
							p_hit_tot += (1 - exp(-Aij))*((ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))*(1./it->second)/probNorm);
					}
					else if(meta_cache == fixP)
					{
						// "partial_sum" only in the exponent with Tc2-Tc1
						Aij += (1-q)*(1-exp(-ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh])) +
								exp(-ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh]) *
								(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID]) * max(double(0), tcVect[node_ID] - tcVect[neigh]) + partial_sum)));
						// "partial_sum" in each exponent.
						//Aij += (1-q)*(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh] + partial_sum))) +
						//		exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID])*tcVect[neigh] + partial_sum)) *
						//		(1-exp(-(ratePrev[neigh][cont_ID]*(1-Pin[neigh][cont_ID]) * max(double(0), tcVect[node_ID] - tcVect[neigh]) + partial_sum)));


						p_hit_tot += ((q * Aij) / (1 - (1-q)*Aij))*((ratePrev[neigh][cont_ID]*(1-Phit[neigh][cont_ID]))/probNorm);
					}
					else
					{
						dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
						exit(0);
					}
				}
			}
		}
		if (clientVect[node_ID])  // we have to calculate the conditional probability with respect to the client
		{
			double Aij = 0;
			double partial_sum = 0;
			int neigh;
			//for (uint32_t j=0; j < neighMat[node_ID][cont_ID].size(); j++)
			if (neighMat[node_ID].size() > 0)
			{
				for (std::map<int,int>::iterator it = neighMat[node_ID].begin(); it!=neighMat[node_ID].end(); ++it)
				{
					neigh = it->first;

					if(ratePrev[neigh][cont_ID]!=0)
						partial_sum += ((ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*tcVect[node_ID])*(1./it->second);
						//partial_sum += (ratePrev[neigh][cont_ID])*(1-Pin[neigh][cont_ID])*max((double)0, tcVect[node_ID]-tcVect[neigh]);
				}
			}

			if(meta_cache == LCE)
			{
				Aij += ((lambda_ex_cont_ID)*tcVect[node_ID] + partial_sum);
				//if(Aij < 0.01)
				//	p_hit_tot += (Aij)*(lambda_ex_cont_ID/probNorm);
				//else
					p_hit_tot += (1 - exp(-Aij))*(lambda_ex_cont_ID/probNorm);
			}
			else if (meta_cache == fixP)
			{
				Aij += q * (1 - exp(-(lambda_ex_cont_ID*tcVect[node_ID] + partial_sum)));
				p_hit_tot += (Aij / (exp(-(lambda_ex_cont_ID*tcVect[node_ID] + partial_sum)) + Aij)) * (lambda_ex_cont_ID/probNorm);
			}
			else
			{
				dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
				exit(0);
			}
		}
    }

	return p_hit_tot;
}


// *** APPROX NRR ***


void statistics::cacheFillModel_Scalable_Approx_NRR(const char* phase)
{
	chrono::high_resolution_clock::time_point tStartAfterFailure;
	chrono::high_resolution_clock::time_point tEndAfterFailure;
	if(strcmp(phase,"init")!=0)
		tStartAfterFailure = chrono::high_resolution_clock::now();
	//long M = (long)content_distribution::zipf.get_catalog_card();		// Catalog cardinality.
	// ** ACCURACY BOOST - START**
	long M = (long)content_distribution::zipf[0]->get_catalog_card();		// Catalog cardinality.
	// ** ACCURACY BOOST - END**
	int N = num_nodes;												// Number of nodes.
	double Lambda = clients[0]->getLambda();						// Aggregate lambda of exogenous requests.
	uint32_t cSize_targ = (double)caches[0]->get_size();			// Target Cache size.

	dbAk << "Model CACHE SIZE = " << cSize_targ << endl;

	//double alphaVal = content_distribution::zipf.get_alpha();
	//double normConstant = content_distribution::zipf.get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - START**
	double alphaVal = content_distribution::zipf[0]->get_alpha();
	double normConstant = content_distribution::zipf[0]->get_normalization_constant();  // Zipf's normalization constant.
	// ** ACCURACY BOOST - END**

	string forwStr = caches[0]->getParentModule()->par("FS");
	dbAk << "*** Model Forwarding Strategy : " << forwStr << " ***" << endl;

	// Retrieve meta-caching algorithm
	DecisionPolicy* decisor = caches[0]->get_decisor();
	Always* dp1 = dynamic_cast<Always*> (decisor);
	if(dp1)
	{
		meta_cache = LCE;
		dpString = "LCE";
		dbAk << "*** LCE *** Decision Policy!" << endl;
	}
	else
	{
		Fix* dp2 = dynamic_cast<Fix*> (decisor);
		if(dp2)
		{
			meta_cache = fixP;
			q = dp2->p;
			dpString = "fixP";
			dbAk << "*** fixP *** Decision Policy with q = " << q << endl;
		}
		else
		{
			dbAk << "Decision policy not supported!" << endl;
			exit(0);
		}
	}

	dbAk << "***** CACHE FILLING WITH MODEL *****" << endl;

	// Definition and initialization of useful data structures.
	float **prev_rate;		// Request rate for each content at each node at the 'Previous Step'.
	float **curr_rate;		// Request rate for each content at each node at the 'Current Step'.
	float **p_in;			// Pin probability for each content at each node.
	float **p_hit;			// Phit probability for each content at each node.

	// All the previous structures will be matrices of size [M][N].
	prev_rate = new float*[N];
	curr_rate = new float*[N];
	p_in = new float*[N];
	p_hit = new float*[N];

	for (int i=0; i < N; i++)
	{
		prev_rate[i] = new float[M];
		curr_rate[i] = new float[M];
		p_in[i] = new float[M];
		p_hit[i] = new float[M];
	}

	double *tc_vect = new double[N]; 		// Vector containing the 'characteristic times' of each node.


	float **p_in_temp;				// Temp vector to find the max p_in per each node at the end of each iteration.
	p_in_temp = new float*[N];
	for(int n=0; n < N; n++)
			p_in_temp[n] = new float[M];

	double *pHitNode = new double[N];
	fill_n(pHitNode,N,0.0);
	double prev_pHitTot = 0.0;
	double curr_pHitTot = 0.0;


	double *sumCurrRate = new double[N];	// It will contain the total incoming rate for each node.
	fill_n(sumCurrRate,N,0.0);

	//vector<int> inactiveNodes;
	vector<int> activeNodes;

	// *** DISABLED FOR PERF EVALUATION ***
	int **steadyCache; 			// Matrix containing the IDs of the content the will be cached inside the active nodes for the hot start.
	steadyCache = new int*[N];
	for (int n=0; n < N; n++)
	{
		steadyCache[n] = new int[cSize_targ];
		fill_n(steadyCache[n], cSize_targ, M+1);
	}

	// *** INITIALIZATION ***
	// At step '0', the incoming rates at each node are supposed to be the same,
	// i.e., prev_rate(i,n) = zipf(i) * Lambda.
	// As a consequence, the characteristic times of all the nodes will be initially the same, and so the Pin.

	int step = 0;
	double num = 0;

	dbAk << "Iteration # " << step << " - INITIALIZATION" << endl;

	for (long m=0; m < M; m++)
	{
		num = (1.0/pow(m+1,alphaVal));
		prev_rate[0][m] = (float)(num*normConstant)*Lambda;
		sumCurrRate[0] += prev_rate[0][m];
	}

	for (int n=1; n < N; n++)
	{
		std::copy(&prev_rate[0][0], &prev_rate[0][M], prev_rate[n]);
		sumCurrRate[n] = sumCurrRate[0];
	}


	// The Tc will be initially the same for all the nodes, so we pass just the first column of the prev_rate.
	// In the following steps it will be Tc_val(n) = compute_Tc(...,prev_rate, n-1).

	double tc_val = compute_Tc_single_Approx(cSize_targ, alphaVal, M, prev_rate, 0, dpString, q);

	dbAk << "Computed Tc during initialization:\t" << tc_val << endl;

	if(meta_cache == LCE)
	{
		for (int n=0; n < N; n++)
		{
			tc_vect[n] = tc_val;
			for (long m=0; m < M; m++)
			{
				p_in[n][m] = 1 - exp(-prev_rate[n][m]*tc_vect[n]);
				p_hit[n][m] = p_in[n][m];
				pHitNode[n] += (prev_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}
			prev_pHitTot += pHitNode[n];
		}
	}

	else if (meta_cache == fixP)
	{
		for (int n=0; n < N; n++)
		{
			tc_vect[n] = tc_val;
			for (long m=0; m < M; m++)
			{
				p_in[n][m] = (q * (1.0 - exp(-prev_rate[n][m]*tc_vect[n])))/(exp(-prev_rate[n][m]*tc_vect[n]) + q * (1.0 - exp(-prev_rate[n][m]*tc_vect[n])));
				p_hit[n][m] = p_in[n][m];
				pHitNode[n] += (prev_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}
			prev_pHitTot += pHitNode[n];
		}
	}
	else
	{
		dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
		exit(0);
	}

	prev_pHitTot /= N;
	dbAk << "pHit Tot Init - " << prev_pHitTot << endl;


	// *** ITERATIVE PROCEDURE ***

	// Cycle over more steps
	int slots = 30;

	//double **MSD; 			// matrix[slots][N]; it will contain computed Mean Square Distance at each step of the iteration.
							// Useful as a stop criterion for the iterative procedure.

	vector<vector<map<int,int> > >  neighMatrix;
	neighMatrix.resize(N);
	for(int n=0; n < N; n++)
		neighMatrix[n].resize(M);


	/*vector<vector<map<int,double> > > rateNeighMatrix;
	rateNeighMatrix.resize(N);
	for(int n=0; n < N; n++)
		rateNeighMatrix[n].resize(M);
	 */

	bool* clientVector = new bool[N];					// Vector indicating if the current node has an attached client.
	for (int n=0; n < N; n++)
		clientVector[n] = false;

	//int* repoVector = new int[M];						// Vector indicating the repoID for each content (e.g., repoVect[2] = 30
														// means that content '2' is stored in Repo '30').
														// To do: transform in vector<> in case of multiple seed copies.

	vector<vector<int> > repoVector;
	repoVector.resize(M);



    int l;

    for (long m=0; m < M; m++)		// CONTENTS
    {
    	// Retrieve the Repository storing content 'm'

    	repo_t repo = __repo(m+1);    // The repo is the one associated to the position of the '1' inside the string
    	l = 0;
    	while (repo)
    	{
    		if (repo & 1)
			{
 				//repoVector[m] = content_distribution::repositories[l];  // So far, the replication of seed copies is supposed
    																    // to be 1 (i.e., the seed copy of each content is stored only in one repo)
    			repoVector[m].push_back(content_distribution::repositories[l]);	// In case of more repos stories the seed copies of a content.
 				//if (m<20)
 				//	dbAk << "The Repo of content # " << m << " is: " << repoVector[m] << endl;
    		}
    		repo >>= 1;
    		l++;
    	}
    }

	int outInt;
	int target;
	int numPotTargets;

	std::vector<int>::iterator itRepoVect;

	for (int n=0; n < N; n++)  // NODES
	{
	    for(int i=0; i < num_clients; i++)
	    {
	    	if(clients_id[i] == n)
	    	{
	    		clientVector[n] = true;
	    		break;
	    	}
	    }

		for (long m=0; m < M; m++)
		{
			//if(n != repoVector[m])  // For the node owning the repo of content 'm' there is no potential target
			//for(itRepoVect = repoVector[m].begin(); itRepoVect != repoVector[m].end(); ++itRepoVect )
			itRepoVect = find (repoVector[m].begin(), repoVector[m].end(), n);
			if (itRepoVect != repoVector[m].end() )
				continue;
			else
			{
				for(itRepoVect = repoVector[m].begin(); itRepoVect != repoVector[m].end(); ++itRepoVect)
				{
					//outInt = cores[n]->getOutInt(repoVector[m]);
					outInt = cores[n]->getOutInt(*itRepoVect);
					target = caches[n]->getParentModule()->gate("face$o",outInt)->getNextGate()->getOwnerModule()->getIndex();
					neighMatrix[target][m].insert( pair<int,int>(n,1));
				}
			}
		}
	}


	// Iterations
	for (int k=0; k < slots; k++)
	{
		step++;
		dbAk << "Iteration # " << step << endl;

		// *** With NRR, we need to renew the neighMatrix at each step
		if (step > 1 && forwStr.compare("nrr") == 0)
		{
			// Reset the old neighMatrix
			for(int n=0; n < N; n++)
			{
				for (int m=0; m < M; m++)
				{
					neighMatrix[n][m].clear();
				}
			}

			for (int n=0; n < N; n++)  // NODES
			{
				if (tc_vect[n] != numeric_limits<double>::max())    // Only active nodes can have potential targets.
				{
					//if (n==11 || n==3)
					//	caches[n]->dump();
					strategy_layer* strategy_ptr = cores[n]->get_strategy();
					int nOutInt = strategy_ptr->__get_outer_interfaces();
					bool *outInterfaces = new bool [nOutInt];
					fill_n(outInterfaces, nOutInt, false);

					for (int m=0; m < M; m++)
					{
						//if(n != repoVector[m]) // For the node owing the repo of content 'm' there is no potential target.
						itRepoVect = find (repoVector[m].begin(), repoVector[m].end(), n);
						if (itRepoVect != repoVector[m].end() )
							continue;
						//if (itRepoVect != repoVector[m].end() && *itRepoVect != n)
						else
						{
							// Reset the vector representing the out interfaces
							fill_n(outInterfaces, nOutInt, false);
							numPotTargets = 0;

							outInterfaces = strategy_ptr->exploit_model(m);  // Lookup inside network caches

							for (int p=0; p < nOutInt; p++)			// Count the number of potential targets.
							{
								if(outInterfaces[p])
									numPotTargets++;
							}


							for (int p=0; p < nOutInt; p++)  	// Fill the neighMatrix for the selected target.
							{
								if(outInterfaces[p])
								{
									target = caches[n]->getParentModule()->gate("face$o",p)->getNextGate()->getOwnerModule()->getIndex();
									neighMatrix[target][m].insert( pair<int,int>(n,numPotTargets));
								}
							}
						}
					}
				}
			}

			/*for (int i=0; i<=2; i++)
			{
				dbAk << "# Neigh for Content # " << i << " : " << neighMatrix[3][i].size() << endl;
				for (std::map<int,int>::iterator it = neighMatrix[3][i].begin(); it!=neighMatrix[3][i].end(); ++it)
				{
					int neigh = it->first;
					dbAk << "Neigh = " << neigh << endl;
				}
			}*/

		}

		// Calculate the 'current' request rate for each content at each node.
		for (int n=0; n < N; n++)			// NODES
		{
			double sum_curr_rate = 0;
			double sum_prev_rate = 0;

			//dbAk << "NODE # " << n << endl;
			for (long m=0; m < M; m++)		// CONTENTS
			{
				double neigh_rate = 0;			// Cumulative miss rate from neighbors for the considered content.

			    if (neighMatrix[n][m].size() > 0 || clientVector[n])	// Sum the miss streams of the neighbors for the considered content.
			    {
			    	//for (uint32_t i=0; i < neighMatrix[n][m].size(); i++)
			    	for (std::map<int,int>::iterator it = neighMatrix[n][m].begin(); it!=neighMatrix[n][m].end(); ++it)
			    	{

			    		// The miss stream of the selected neighbor will depend on its hit probability for the selected
			    		// content (i.e., Phit(neigh,m). This can be calculated using the conditional probabilities
			    		// Phit(neigh,m|j) = 1 - exp(-A_neigh_j). It expresses the probability that a request for 'm'
			    		// hits cache 'neigh', provided that it comes from cache 'j'. Only neighboring caches for which
			    		// the neigh represents the next hop will be considered.
			    		//int neigh = neighMatrix[n][m][i];
			    		int neigh = it->first;
			    		int numPot = it->second;    // number of potential targets for the neighbor

			    		/*if (step == 1 && n == 4 )
			    		{
			    			dbAk << "NODE # " << n << ", Content # " << m << " Neigh # " << neigh << " Num Pot Targ # " << numPot << endl;
			    		}*/
			    		if(meta_cache == LCE)
			    		{
			    			if(prev_rate[neigh][m]*tc_vect[neigh] <= 0.01)
			    				p_hit[neigh][m] = prev_rate[neigh][m]*tc_vect[neigh];
			    			else
			    				p_hit[neigh][m] = calculate_phit_neigh(neigh, m, prev_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
			    		}
			    		else if(meta_cache == fixP)
			    		{
			    			if(q*prev_rate[neigh][m]*tc_vect[neigh] <= 0.01)
			    				p_hit[neigh][m] = q*(prev_rate[neigh][m]*tc_vect[neigh]);
			    			else
			    				p_hit[neigh][m] = calculate_phit_neigh(neigh, m, prev_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
			    		}
			    		else
						{
							dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
							exit(0);
						}


						if (p_hit[neigh][m] > 1)
							dbAk << "Node: " << n << "\tContent: " << m << "\tNeigh: " << neigh << "\tP_hit: " << p_hit[neigh][m] << endl;

						//neigh_rate += (prev_rate[neigh][m]*(1-p_hit[neigh][m]));
						neigh_rate += (prev_rate[neigh][m]*(1-p_hit[neigh][m]))*(1./numPot);

						//rateNeighMatrix[n][m][neigh] = 0; 		// Reset the result of the previous iteration.
						//rateNeighMatrix[n][m][neigh] += (prev_rate[neigh][m]*(1-p_hit[neigh][m]));
			    	}
			    }


			    if (clientVector[n])	// In case a client is attached to the current node.
		    	{
					num = (1.0/(double)pow(m+1,alphaVal));
					neigh_rate += (num*normConstant)*Lambda;
		    	}

			    curr_rate[n][m] = neigh_rate;

			    sum_curr_rate += curr_rate[n][m];
			    sum_prev_rate += prev_rate[n][m];

			    prev_rate[n][m] = curr_rate[n][m];

			}  // contents

			//dbAk << "Iteration # " << step << " Node # " << n << " Incoming Rate: " << sum_curr_rate << endl;

			sumCurrRate[n] = sum_curr_rate;

			// Calculate the new Tc and Pin for the current node.
			if (sum_curr_rate != 0) 	// The current node is hit either by exogenous traffic or by miss streams from
										// other nodes (or by both)
			{
				//dbAk << "NODE # " << n << " Sum Current Rate: " << sumCurrRate[n] << endl;
				tc_vect[n] = compute_Tc_single_Approx_More_Repo(cSize_targ, alphaVal, M, curr_rate, n, dpString, q);

				//dbAk << "Node # " << n << " - Tc " << tc_vect[n] << endl;
				if(meta_cache == LCE)
				{
					for(long m=0; m < M; m++)
					{
						if(curr_rate[n][m]*tc_vect[n] <= 0.01)
						{
							p_in[n][m] = curr_rate[n][m]*tc_vect[n];
						}
						else
						{
							p_in[n][m] = 1 - exp(-curr_rate[n][m]*tc_vect[n]);
						}
					}
				}
				else if(meta_cache == fixP)
				{
					for(long m=0; m < M; m++)
					{
						if(q*curr_rate[n][m]*tc_vect[n] <= 0.01 && q*curr_rate[n][m]*tc_vect[n] > 0.0)
						{
							p_in[n][m] = q * curr_rate[n][m]*tc_vect[n];
						}
						else if(q*curr_rate[n][m]*tc_vect[n] == 0.0)
						{
							p_in[n][m] = 0;
							p_hit[n][m] = 0;
						}
						else
						{
							p_in[n][m] = (q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])))/(exp(-curr_rate[n][m]*tc_vect[n]) + q * (1.0 - exp(-curr_rate[n][m]*tc_vect[n])));
						}
					}
				}
				else
				{
					dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
					exit(0);
				}

				// Calculate the Phit of the node hosting one of the repos (it can happen that it does not forward any
				// traffic, thus it is not a neighbor for any node, and so the logic of the p_hit calculus followed
				// before does not apply.
				if (find (content_distribution::repositories, content_distribution::repositories + num_repos, n)
							!= content_distribution::repositories + num_repos)
				{
					for(long m=0; m < M; m++)
					{
						if(meta_cache == LCE)
						{
							if(curr_rate[n][m]*tc_vect[n] <= 0.01)
								p_hit[n][m] = curr_rate[n][m]*tc_vect[n];
							else
								p_hit[n][m] = calculate_phit_neigh(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else if(meta_cache == fixP)
						{
							if(q*curr_rate[n][m]*tc_vect[n] <= 0.01 && q*curr_rate[n][m]*tc_vect[n] > 0.0)
								p_hit[n][m] = q*(curr_rate[n][m]*tc_vect[n]);
							else if(q*curr_rate[n][m]*tc_vect[n] == 0.0)
								p_hit[n][m] = 0;
							else
								p_hit[n][m] = calculate_phit_neigh(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else
						{
							dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
							exit(0);
						}
					}
				}
				// Do the same thing for the temporary repos
				for(long m=0; m < M; m++)
				{
					itRepoVect = find (repoVector[m].begin(), repoVector[m].end(), n);
					if (itRepoVect != repoVector[m].end() )
					{
						if(meta_cache == LCE)
						{
							if(curr_rate[n][m]*tc_vect[n] <= 0.01)
								p_hit[n][m] = curr_rate[n][m]*tc_vect[n];
							else
								p_hit[n][m] = calculate_phit_neigh(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else if(meta_cache == fixP)
						{
							if(q*curr_rate[n][m]*tc_vect[n] <= 0.01 && q*curr_rate[n][m]*tc_vect[n] > 0.0)
								p_hit[n][m] = q*(curr_rate[n][m]*tc_vect[n]);
							else if(q*curr_rate[n][m]*tc_vect[n] == 0.0)
							{
								p_hit[n][m] = 0;
								p_in[n][m] = 0;
							}
							else
								p_hit[n][m] = calculate_phit_neigh(n, m, curr_rate, p_in, p_hit, tc_vect, alphaVal, Lambda, M, clientVector, neighMatrix);
						}
						else
						{
							dbAk << "Meta Caching Algorithm NOT Implemented!" << endl;
							exit(0);
						}
					}
				}

			}
			else		// It means that the current node will not be hit by any traffic.
						// So we zero the miss stream coming from this node by putting p_in=1 for each content.
			{
				tc_vect[n] = numeric_limits<double>::max();   // Like infinite value;
				//dbAk << "Iteration # " << step << " NODE # " << n << " Tc - " << tc_vect[n] << endl;
				for(long m=0; m < M; m++)
				{
					p_in[n][m] = 0;
					p_hit[n][m] = 0;
				}

			}

		} // nodes

		curr_pHitTot = 0;
		for(int n=0; n < N; n++)
		{
			pHitNode[n] = 0;
			if(sumCurrRate[n]!=0)
			{
				for (long m=0; m < M; m++)
				{
					pHitNode[n] += (curr_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
				}
				curr_pHitTot += pHitNode[n];
			}

		}

		// *** EXIT CONDITION ***
		if( (abs(curr_pHitTot - prev_pHitTot)/curr_pHitTot) < 0.005 )
			break;
		else		// For the NRR implementation, the actual cache fill is moved here in order to update the neighMatrix
					// computation at the next step
		{
			prev_pHitTot = curr_pHitTot;

			/// ***** TRY *****
			for(int n=0; n < N; n++)
			{
				if(sumCurrRate[n]!=0)
				{
					for (long m=0; m < M; m++)
					{
						curr_rate[n][m] = 0;
						// Clear and re-fill the repo vector with nodes storing seed copies
						repoVector[m].clear();
				    	repo_t repo = __repo(m+1);    // The repo is the one associated to the position of the '1' inside the string
				    	l = 0;
				    	while (repo)
				    	{
				    		if (repo & 1)
							{
				 				//repoVector[m] = content_distribution::repositories[l];  // So far, the replication of seed copies is supposed
				    																    // to be 1 (i.e., the seed copy of each content is stored only in one repo)
				    			repoVector[m].push_back(content_distribution::repositories[l]);	// In case of more repos stories the seed copies of a content.
				 				//if (m<20)
				 				//	dbAk << "The Repo of content # " << m << " is: " << repoVector[m] << endl;
				    		}
				    		repo >>= 1;
				    		l++;
				    	}

					}
				}
			}


			// *** NRR
			// **** DISABLED ****
			if(forwStr.compare("nrr") == 0)
			{
				int maxPin;

				activeNodes.clear();

				for(int n=0; n < N; n++)
				{
					// Allocate contents only for nodes interested by traffic (it depends on the frw strategy)
					if(tc_vect[n] != numeric_limits<double>::max())
					{
						// **** NBB *** manca l'incremento degli active nodes!!!!
						activeNodes.push_back(n);

						// *** DISABLED for perf measurements
						// *** DETERMINING CONTENTS TO BE PUT INSIDE CACHES***
						p_in_temp[n] = p_in[n];
						for(uint32_t k=0; k < cSize_targ; k++)
						{
							maxPin = distance(p_in_temp[n], max_element(p_in_temp[n], p_in_temp[n] + M));  // Position of the highest popular object (i.e., its ID).
							if(p_in_temp[n][maxPin] == 0)			// There are few contents than cSize_targ that can be inside the cache
								break;
							steadyCache[n][k] = maxPin;

							// Adding temporary cached copies to the repo vector
							repoVector[maxPin].push_back(n);

							//p_in_temp[n][k] = p_in[n][maxPin];
							//p_hit_temp[n][k] = p_hit[n][maxPin];
							p_in_temp[n][maxPin] = 0;
						}
					}

					caches[n]->flush();
				}

				// *** REAL CACHE FILLING ***
				if(strcmp(phase,"init")==0)
				{
					dbAk << "Filling Caches with Model results...\n" << endl;
					int node_id;
					uint32_t cont_id;

					// In case of FIX probabilistic with need to temporarly change the caching prob id order to fill the cache
					DecisionPolicy* decisor;
					Fix* dp1;

					ccn_data* data = new ccn_data("data",CCN_D);

					for(unsigned int n=0; n < activeNodes.size(); n++)
					{
						node_id = activeNodes[n];

						decisor = caches[node_id]->get_decisor();
						dp1 = dynamic_cast<Fix*> (decisor);;

						if(dp1)
						{
							dp1->setProb(1.0);
						}

						// Empty the cache from the previous step
						caches[node_id]->flush();

						for (unsigned int k=0; k < cSize_targ; k++)
						{
							cont_id = (uint32_t)steadyCache[node_id][k] + 1;
							if (cont_id == M+2)			// There are few contents than cSize_targ that can be inside the cache
								break;
							chunk_t chunk = 0;
							__sid(chunk, cont_id);
							__schunk(chunk, 0);
							//ccn_data* data = new ccn_data("data",CCN_D);
							data -> setChunk (chunk);
							data -> setHops(0);
							data->setTimestamp(simTime());
							caches[node_id]->store(data);

							// Adding the location of the new cached content as a repo for that content
			    			// in order to avoid considering outgoing flows from that node and for that
							// particular content.
						}
						//dbAk << "Cache Node # " << node_id << " Step # " << step << " :\n";
						//caches[node_id]->dump();

						if(dp1)
						{
							dp1->setProb(q);
						}


						// Reset the steadyCache vector
						fill_n(steadyCache[n], cSize_targ, M+1);
					}

					delete data;
				}
			}
		}
	}  // Successive step

	int maxPin;

	double pHitTotMean = 0;
	double pHitNodeMean = 0;

	activeNodes.clear();

	for(int n=0; n < N; n++)
	{
		// Allocate contents only for nodes interested by traffic (it depends on the frw strategy)
		if(tc_vect[n] != numeric_limits<double>::max())
		{
			activeNodes.push_back(n);

			// Calculate di p_hit mean of the node
			for(uint32_t m=0; m < M; m++)
			{
				pHitNodeMean += (curr_rate[n][m]/sumCurrRate[n])*p_hit[n][m];
			}

			// *** DISABLED for perf measurements
			if(!onlyModel)
			{
			// *** DETERMINING CONTENTS TO BE PUT INSIDE CACHES***
			// Choose the contents to be inserted into the cache.
				for(uint32_t k=0; k < cSize_targ; k++)
				{
					maxPin = distance(p_in[n], max_element(p_in[n], p_in[n] + M));  // Position of the highest popular object (i.e., its ID).
					// *** NRR
					if(p_in[n][maxPin] == 0.0)			// There are few contents than cSize_targ that can be inside the cache
						break;
					steadyCache[n][k] = maxPin;

					//dbAk << "NODE # " << n << " Content # " << maxPin << endl;

					//p_in_temp[n][k] = p_in[n][maxPin];
					//p_hit_temp[n][k] = p_hit[n][maxPin];
					p_in[n][maxPin] = 0;
				}
			}


			pHitTotMean += pHitNodeMean;
			dbAk << "NODE # " << n << endl;
			dbAk << "-> Mean Phit: " << pHitNodeMean << endl;
//			dbAk << "-> Steady Cache: Content - Pin - Phit\n";
//			for (uint32_t k=0; k < cSize_targ; k++)
//				dbAk << steadyCache[n][k] << " - " << p_in_temp[n][k] << " - " << p_hit_temp[n][k] << endl;
			dbAk << endl;
			pHitNodeMean = 0;
		}
	}

	if(strcmp(phase,"init")==0)
		dbAk << "Number of Active Nodes 1: " << activeNodes.size() << endl;
	else
		dbAk << "Number of Active Nodes 2: " << num_nodes - activeNodes.size() << " Time: " << simTime() << endl;

	pHitTotMean = pHitTotMean/(activeNodes.size());
	//pHitTotMean = pHitTotMean/(N);

	if(strcmp(phase,"init")==0)
		dbAk << "MODEL - P_HIT MEAN TOTAL AFTER CACHE FILL: " << pHitTotMean << endl;
	else
		dbAk << "MODEL - P_HIT MEAN TOTAL AFTER ROUTE re-CALCULATION: " << pHitTotMean << endl;

	dbAk << "*** Tc of nodes ***\n";
	for(int n=0; n < N; n++)
	{
		dbAk << "Tc-" << n << " --> " << tc_vect[n] << endl;
	}

    // *** DISABLED per perf evaluation
	if(!onlyModel)
	{
		// *** REAL CACHE FILLING ***
		if(strcmp(phase,"init")==0)
		{
			dbAk << "Filling Caches with Model results...\n" << endl;
			int node_id;
			uint32_t cont_id;

			// In case of FIX probabilistic with need to temporarly change the caching prob id order to fill the cache
			DecisionPolicy* decisor;
			Fix* dp1;

			ccn_data* data = new ccn_data("data",CCN_D);

			for(unsigned int n=0; n < activeNodes.size(); n++)
			{
				node_id = activeNodes[n];

				decisor = caches[node_id]->get_decisor();
				dp1 = dynamic_cast<Fix*> (decisor);;

				if(dp1)
				{
					dp1->setProb(1.0);
				}

				// Empty the cache from the previous step.
				caches[node_id]->flush();

				//for (int k=cSize_targ-1; k >= 0; k--)   // Start inserting contents with the smallest p_in.
				for (unsigned int k=0; k < cSize_targ; k++)
				{
					cont_id = (uint32_t)steadyCache[node_id][k]+1;
					if (cont_id == M+2)			// There are few contents than cSize_targ that can be inside the cache
						break;
					chunk_t chunk = 0;
					__sid(chunk, cont_id);
					__schunk(chunk, 0);
					//ccn_data* data = new ccn_data("data",CCN_D);
					data -> setChunk (chunk);
					data -> setHops(0);
					data->setTimestamp(simTime());
					caches[node_id]->store(data);
				}
				//dbAk << "Cache Node # " << node_id << " :\n";
				//caches[node_id]->dump();

				if(dp1)
				{
					dp1->setProb(q);
				}

			}

			delete data;
		}
	}



	// DISABLED for perf eval
	// *** INCOMING RATE CALCULATION rate for each node on each link
	/*double linkTraffic = 0;
	int neigh;
	vector<map<int,double> > totalNeighRate;
	totalNeighRate.resize(N);
	dbAk << "*** Link State ***" << endl;
	//dbAk << "Extreme A -- Link Traffic -- Extreme B" << endl;
	dbAk << "Extreme A -- Link Load [Bw=1Mbps] -- Extreme B" << endl;
	dbAk << endl;
	for (int n=0; n < N; n++)
	{
		for (int m=0; m < M; m++)
		{
			map<int,double>::iterator it = rateNeighMatrix[n][m].begin();
			while(it!=rateNeighMatrix[n][m].end())
			{
				neigh = it->first;
				map<int,double>::iterator itInner = totalNeighRate[n].find(neigh);
				if(itInner == totalNeighRate[n].end()) // Insert for the first time
					totalNeighRate[n][neigh] = it->second;
				else										// Neigh already inserted before.
					totalNeighRate[n][neigh] += it->second;
				++it;
			}
		}
	}

	for (int n=0; n < N; n++)
	{
		for(map<int,double>::iterator it=totalNeighRate[n].begin(); it!=totalNeighRate[n].end(); ++it)
		{
			neigh = it->first;
			linkTraffic += it->second;
			map<int,double>::iterator itInner = totalNeighRate[neigh].find(n);  // Sum the traffic in the opposite direction
			if(itInner != totalNeighRate[neigh].end())  // There is also traffic in the opposite direction.
			{
				linkTraffic += itInner->second;
				totalNeighRate[neigh].erase(itInner);
			}
			//dbAk << "Node " << n << "\t- " << linkTraffic << " [Interest/s] -\tNode " << neigh << endl;
			dbAk << "Node " << n << "\t- " << (double)((linkTraffic*(1536*8))/1000000) << " -\tNode " << neigh << endl;
			linkTraffic = 0;

			/* Take the DataRate from each Link; To use with cDatarateChannel and not with the current cDelayChannel
			cDelayChannel *chPointF;
			for (int i = 0; i<caches[n]->getParentModule()->gateSize("face$o");i++)
			{
				if (!caches[n]->__check_client(i))
				{
					int index = caches[n]->getParentModule()->gate("face$o",i)->getNextGate()->getOwnerModule()->getIndex();
					if(index==neigh)
					{
						chPointF =  dynamic_cast<cDelayChannel*> (caches[n]->getParentModule()->gate("face$o",i)->getChannel());
						if(chPointF)
							dbAk << "Node # " <<  n << " Datarate link at interface " << i << " : " << chPointF->getNominalDatarate() << endl;

					}
				}
			}*/
//		}
//	}

	if(strcmp(phase,"init")!=0)
	{
		tEndAfterFailure = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>( tEndAfterFailure - tStartAfterFailure  ).count();
		dbAk << "Execution time of the model after failure [ms]: " << duration << endl;
	}
	// De-allocating memory
	for (int n = 0; n < N; ++n)
	{
		delete [] prev_rate[n];
		delete [] curr_rate[n];
		delete [] p_in[n];
		delete [] p_hit[n];
	}

	delete [] prev_rate;
	delete [] curr_rate;
	delete [] p_in;
	delete [] p_hit;

}
