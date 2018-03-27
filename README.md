````
		  / _____)_	           
  ____ ____ ____ ( (____ (_)_ __ ___ 
 / ___) ___)  _ \ \____ \| | '_ ` _ \  
( (__( (___| | | |_____) | | | | | | | 
 \____)____)_| |_(______/|_|_| |_| |_|				

````

# Welcome

Thank you for joining the ccnSim community!  

This is ccnSim-0.4-Parallel, the new ccnSim version which implements in addition to:
* the classical Event Driven (ED) engine [1]
* the hybrid stochastic/Monte-Carlo simulation engine known as ModelGraft (MG) [2], that allow 100x gains with respect to ED
* a new parallel engine, known as CS-POST-MT [3] that allows gains of 100x with respect to MG, and thus 10000x with respect to ED!

## Where to start

Note that https://github.com/TeamRossi/ccnSim-0.4-Parallel only specifically deals with the parallel version of ccnSim, and assume users are already familiar with ccnSim (and hopefully with both ED and MG engines). It also assumes users to have at least quickly gone through the technical report describing the new parallel CS-POST-MT engine [3].

Readers interested in the single-threaded stable version of ccnSim-0.4 (ED or MG engines), should instead check https://github.com/TeamRossi/ccnSim-0.4  if need access to the source code, or directly use the docker container version  https://hub.docker.com/r/nonsns/ccnsim-0.4/ 

For information about installation and sample scenarios, please follow the detailed instructions provided in this README.md (as well as in the the ccnSim-Parallel "User Manual", available in the Manual/ folder of this Git https://github.com/TeamRossi/ccnSim-0.4-Parallel/blob/master/Manual/Parallel_ModelGraft_Manual.pdf )  Please also read the documentation inside the provided "runsim_script_Parallel_ModelGraft.sh" script in order to have a clear understanding of the simulation process. 

Finally, please note that simulations with ccnSim-0.4-Parallel require a patched version of the Akaroa2 software [4, 5]. While the patch is freely distributed in this Git, users should obtain a valid license in order to download Akaroa-2.7.13 (i.e., 
the version used for the development of ccnSim-0.4-Parallel) to which the patch can be applied.  For info related to the various types of license please visit the Akaroa Project Website at [4]. 

Specifically, the files we had to modify to suit our purpose are the following
+src/engine/ak_observation.C
+src/include/observation.H
+src/engine/observation_handler.H
+src/engine/observation_analyzer.C
+src/engine/observation_analyzer.H
+src/engine/parameter_analyzer.C
+src/engine/parameter_analyzer.H
+src/engine/spectral/sa_variance_estimator.C
+src/engine/spectral/sa_variance_estimator.H
+src/include/checkpoint.H
+src/include/checkpoint.C
+src/ipc/connection.H
+src/akmaster/engine.H
+src/akmaster/engine_connection.C
+src/akmaster/engine.C
+src/akmaster/simulation.C
+src/akmaster/simulation.H



## References

A few references to the ED [1], ModelGraft [2] and Parallel (CS-POST-MT) [3] engines:

[1] Chiocchetti, Raffaele, Rossi, Dario and Rossini, Giuseppe, 
"ccnSim: an Highly Scalable CCN Simulator"
In IEEE International Conference on Communications (ICC), June 2013.

[2] M. Tortelli, D. Rossi, E. Leonardi, 
"A Hybrid Methodology for the Performance Evaluation of Internet-scale Cache Networks", 
Elsevier Computer Networks, 2017, DOI: 10.1016/j.comnet.2017.04.006.

[3] Tortelli, Michele, Rossi, Dario and Leonardi, Emilio,
"Parallel Simulation of Very Large-Scale General Cache Networks"
http://www.enst.fr/~drossi/paper/rossi17parallel-cache.pdf, November 2017.

	
[4] Akaroa Project Website, https://akaroa.canterbury.ac.nz/akaroa/.    

[5] G. Ewing, K. Pawlikowski, and D. McNickle. Akaroa-2: Exploiting network computing by distributingstochastic simulation. SCSI Press, 1999.



# Instruction to install ccnSim-Parallel

In this section we will cover all the steps needed for the installation of ccnSim-Parallel. We suggest the reader to check and execute them by following the presentation order.

## Portability

ccnSim-Parallel has been tested on the following platforms, and with the following softwares:
* Ubuntu Linux 14.04 (64-bit)
* Ubuntu Linux 16.04 Server (64-bit)
* Omnetpp-4.6
* Omnetpp-5.0 
* Akaroa-2.7.13

## Prerequisites
* Boost libraries ≥ 1.54

They can be installed either by using the standard packet manager of your system (e.g., apt-get install, yum install, port install, etc.), or by downloading them from http://www.boost.org/users/download/, and following instructions therein.

* gcc > 4.8.1

On Ubuntu platforms, gcc can be updated by adding the ubuntu-toolchain-r/test PPA. Sample commands for the latest 5.x version are:
    
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt update
    sudo apt install gcc-5 g++-5
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 60 --slave /usr/bin/g++ g++ /usr/bin/g++-5

• parallel-ssh

In order to ease the simulation workflow, a passwordless SSH login needs to be present on all the available servers. parallel-ssh can than be used in order to send commands to slaves from the master node.

    sudo apt-get install pssh

## Akaroa2

As mentioned above, Akaroa2 requires a specific license in order to be used. For instructions about software download and licenses, please visit the website https://akaroa.canterbury. ac.nz/akaroa/. ccnSim-Parallel has been developed starting from Akaroa-2.7.13. The whole set of modifications that have been made in order to implement the parallel ModelGraft strategy are grouped inside the Akaroa_2.7.13_for_PMG.patch.
Once obtained the original licensed Akaroa-2.7.13, place it in the same directory with the aforementioned patch, and type:

    patch -s -p0 < Akaroa_2.7.13_for_PMG.patch

Notice that the original Akaroa directory will keep the same name even after the application of the patch.
Once the patch is applied, type the following commands from the root directory of the patched Akaroa in order to install it:

    ./configure
    make
    sudo make install
    
Akaroa binaries, libraries, and include files will be installed in /usr/local/akaroa/bin, /usr/local/akaroa/lib, and /usr/local/akaroa/include, respectively. If you do not have sudo privileges, and you need to install Akaroa into another location, e.g., <install_path>, please add this location to your PATH variable, and use the following commands:

    ./configure --prefix=<install_path>
    make
    make install
    
Furthermore, always in the case of lacking sudo privileges, users might need to modify LIB and PYTHON paths in the several “Makefile.config_*” files and inside the “pyconfig.py” file, according to the customized locations of related libraries, before compiling and installing Akaroa.

## Omnet++

ccnSim-Parallel has been tested with both Omnetpp-4.6 and Omnetpp-5.0, which are available at https://omnetpp.org/. Before the installation, Omnetpp-x requires prerequisite packages to be installed. For the complete list, please read the official installation manual. Furthermore, since Omnetpp-x bin/ directory needs to be added to the PATH variable, the following line should be put at the end of the 􏰅/.bashrc file:

    export PATH=$PATH:$HOME/omnetpp-x/bin
    
After having closed and saved the file, please close and re-open the terminal.
Regardless of the Omnetpp version, a couple of patch files need to be applied to it before compilation and installation. These patch files are provided with ccnSim-Parallel, which we suppose being located in $CCNSIM_DIR. Provided that Omnetpp is located in $OMNET_DIR, specific instructions to patch, compile, and install it without the graphical interface (not needed for parallel ModelGraft simulations) are provided in the following, according to the selected version:

* Omnetpp-4.6

      pint:􏰅$ cd $CCNSIM_DIR
      pint:CCNSIM_DIR$ cp ./patch/omnet4x/ctopology.h $OMNET_DIR/include/ 
      pint:CCNSIM_DIR$ cp ./patch/omnet4x/ctopology.cc $OMNET_DIR/src/sim/ 
      pint:CCNSIM_DIR$ cd $OMNET_DIR/
      pint:OMNET_DIR$ NO_TCL=1 ./configure
      pint:OMNET_DIR$ make
￼
* Omnetpp-5.0

      pint:􏰅$ cd $CCNSIM_DIR
      pint:CCNSIM_DIR$ cp ./patch/omnet5x/ctopology.h $OMNET_DIR/include/omnetpp 
      pint:CCNSIM_DIR$ cp ./patch/omnet5x/ctopology.cc $OMNET_DIR/src/sim 
      pint:CCNSIM_DIR$ cd $OMNET_DIR/
      pint:OMNET_DIR$ ./configure WITH_TKENV=no
      pint:OMNET_DIR$ make

Compilation flags can also be set through the file configure.user (e.g., to disable Qtenv).

## ccnSim-Parallel

Installation requires the following commands from the $CCNSIM_DIR:

    ./scripts/makemake.sh
    make
    
If all the steps described in the previous sections have been successfully executed, simulations of general cache networks using the parallel ModelGraft technique can now be launched.  



# Using ccnSim-Parallel

We have provided plenty of scripts and example of usage. Please refer to the PDF Manual for details about the execution of sample scenarios! The PDF manual allow you to repeat experiments of the technical report [3] listed above

https://github.com/TeamRossi/ccnSim-0.4-Parallel/blob/master/ccnSim-0.4-Parallel-Manual.pdf
