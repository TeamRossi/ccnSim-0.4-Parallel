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
#include "content_distribution.h"

#include "statistics.h"

#include "ccn_interest.h"
#include "ccn_data.h"

#include "ccnsim.h"
#include "client_IRM.h"

#include "error_handling.h"
#include <random>

Register_Class (client_IRM);


void client_IRM::initialize()
{
	int num_clients = getAncestorPar("num_clients");
	active = false;
	if (find(content_distribution::clients, content_distribution::clients + num_clients ,getNodeIndex()) != content_distribution::clients + num_clients)
	{
		active = true;
		lambda = getAncestorPar("lambda");
		check_time	= getAncestorPar("check_time");

		timer = new cMessage("timer", TIMER);
		scheduleAt( simTime() + check_time, timer );

		//arrival = new cMessage("arrival", ARRIVAL );
		//scheduleAt( simTime() + uniform(0,1./lambda), arrival);
		//scheduleAt( simTime() + exponential(1./lambda), arrival);
		//scheduleAt( simTime() + 1./lambda, arrival);

		if(!onlyModel)		// The scheduling of clients requests should be done.
		{
			cModule* pSubModule = getParentModule()->getSubmodule("statistics");
			statistics* pStatisticsModule = dynamic_cast<statistics*>(pSubModule);

			unsigned long long M = content_distribution::zipf[0]->get_catalog_card();
			//cout << " ------- Original Cardinality from Client_IRM:\t" << M << " -------" << endl;

			down = pStatisticsModule->par("downsize");
			//cout << " ------- DOWN from Client_IRM from Par:\t" << down << " -------" << endl;

			newCard = round(M*(1./(double)down));    // Equal to the number of bin (or meta-contents)
			//cout << " ------- NewCard from Client_IRM:\t" << newCard << " -------" << endl;

			alphaVal = content_distribution::zipf[0]->get_alpha();

			if(down > 1)		// ModelGraft
			{
				// Schedule a ModelGraft request (i.e., arrival_ttl) at lambda/down rate
				arrival_ttl = new cMessage("arrival_ttl", ARRIVAL_TTL);
				scheduleAt( simTime() + uniform(0,(double)(1./(double)(lambda/down))), arrival_ttl);
			}
			else if(down == 1)		// ED-sim scenario
			{
				arrival = new cMessage("arrival", ARRIVAL );
				scheduleAt( simTime() + uniform(0,1./lambda), arrival);
			}
			else
			{
				std::stringstream ermsg;
				ermsg<<"Please insert a downsizing factor Delta >= 1.";
				severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
			}
		}
		client::initialize();
	}
}

void client_IRM::finish()
{
	client::finish();
}

void client_IRM::handleMessage(cMessage *in)
{
	unsigned long long origContent;
	unsigned long metaContent;

    if (in->isSelfMessage())	// A self-generated message can be either an 'ARRIVAL', an 'ARRIVAL_TTL', or a 'TIMER'.
    {
		switch(in->getKind())
		{
		case ARRIVAL:
			request_file(newCard+1);   // 'newCard+1' is needed to discern between ED-sim and ModelGraft.
			scheduleAt( simTime() + exponential(1./lambda), arrival );
			break;
		case ARRIVAL_TTL:
			// Extract a content from the original catalog (i.e., M cardinality) through the inversion rejection sampling
			origContent = content_distribution::zipf[0]->sample();

			// Compute the correspondent meta-content to be requested (i.e., newCard cardinality)
			metaContent = floor(origContent/down) + 1;

		    if(metaContent > 0 && metaContent <= newCard)
		    {
		    	request_file(metaContent);
		    	scheduleAt( simTime() + exponential(1./(lambda/down)), arrival_ttl);  // Schedule the next request
		    }
		    else
		    {
		    	std::stringstream ermsg;
		    	ermsg<<"ERROR - Client IRM: received wrong self message identifier. Please check";
		    	severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
		    }
		    break;
		case TIMER:
			handle_timers(in);
			scheduleAt( simTime() + check_time, timer );
			break;
		default:
			std::stringstream ermsg_a;
			ermsg_a<<"ERROR - Client IRM: received wrong self message identifier. Please check";
			severe_error(__FILE__,__LINE__,ermsg_a.str().c_str() );
			break;
		}
		return;

    }

    switch (in->getKind())		// In case of an external message, it can only be a DATA packet.
 	{
 		case CCN_D:
 		{
 			#ifdef SEVERE_DEBUG
 			if (!active){
 				std::stringstream ermsg;
 				ermsg<<"Client attached to node "<<getNodeIndex()<<
 					"  received a DATA despite being NOT ACTIVE";
 				severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
 			}
 			#endif

 			ccn_data *data_message = (ccn_data *) in;
 			handle_incoming_chunk (data_message);
 			delete  data_message;
 			break;
 		}

 		#ifdef SEVERE_DEBUG
 		default:
 			std::stringstream ermsg;
 			ermsg<<"Clients can only receive DATA, while this is a message"<<
 				" of TYPE "<<in->getKind();
 			severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
 		#endif
    }
}


/*
 *		Generate Interest packets according to an IRM process. Inter-request times are exponentially distributed
 *		with mean = down/lambda.
 *
 *		Parameters:
 *		- nameC = contentID to be requested (if < newCard+1).
 */
void client_IRM::request_file(unsigned long nameC)
{
	name_t name;

	if(nameC == newCard+1)  // ED-sim
		name = content_distribution::zipf[0]->sample();	// Extract a content from the original catalog (rejection-inversion sampling)
	else					// ModelGraft (TTL-based)
		name = (name_t) nameC;

	struct download new_download = download (0,simTime() );
	#ifdef SEVERE_DEBUG
	new_download.serial_number = interests_sent;

	if (!active){
		std::stringstream ermsg;
		ermsg<<"Client attached to node "<< getNodeIndex() <<" is requesting file but it "
			<<"shoud not as it is not active";
		severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
	}

	if (getNodeIndex() == 18){
		std::stringstream ermsg;
		ermsg<<"I am node "<< getNodeIndex() <<" and I am requesting a file";
		severe_error(__FILE__,__LINE__,ermsg.str().c_str() );
	}
	#endif

	current_downloads.insert(pair<name_t, download >(name, new_download ) );
	//cout << " ** Interest Sent for:\t" << name << endl;
	send_interest(name, 0 ,-1);
}
