# QuickLinx 

A fast, lightweight utility for modifying RSLinx Classic ethernet driver configurations

<img width="666" height="203" alt="be27567e-7ae7-4b67-9146-cd4ee5e28563" src="https://github.com/user-attachments/assets/f2d6f277-ae3b-4819-994b-3ba38a05a5b3" />

## Overview

QuickLinx is a specialized tool designed to increase efficiency when managing RSLinx Classic ethernet driver configurations.

QuickLinx provides a safe, intuitive interface for:
* Exporting existing RSLinx driver configurations into a CSV file
* Importing new configuration files from a CSV file
* Merging imported drivers with the current configuration
* Overwriting configurations entirely when needed

It is built for technicians, controls engineers, and automation professionals who need a rapid and reliable way to quickly manage RSLinx Classic Ethernet Drivers.


## Developer comments: 

    Before following these steps, take into consideration that QuickLinx is in an alpha state. 
    It is recommended that you backup your current Windows Registry so that you have a restore 
    point if any unplanned behavior occurs. 

    Also, as of now, for changes to take effect RSLinx Classic must be fully closed 
    (not even running as a service). For the best experience, use QuickLinx before attempting 
    to connect with controllers using Logix5000, Studio5000, etc. If you find that you did not close 
    RSLinx Classic before using QuickLinx, simply close out of RSLinx and restart it or optionally 
    find it in task manager and kill the process. When you relaunch RSLinx, you will find the updated drivers.

    NOTES: 
      * The only supported driver type is AB_ETH
      * Driver names cannot exceed the 15 character limit
      * Range values must be a valid format 
            * (BAD: 127.0.0, 127.0.0.1.1, 127.0.0.1-, 127.0.0.1-256, 127.0.0.15-4)
            * (GOOD: 127.0.0.1, 127.0.1.1-15, 127.0.0.1-254)
      * Maximum nodes per driver is 254 (255 - 1 as station 63 is reserved)

    -BRD

## How to use
To get started using QuickLinx:
    
1: Close or kill the RSLinx Classic process

2: Create a CSV file of Ethernet Drivers using the following format

    Type,Name,Range
    AB_ETH,Example_One,127.0.0.1
    AB_ETH,Example_Two,127.0.1.1-50
    ...
    
3: Use the import button to stage driver changes. QuickLinx will verify the integrity and format of the imported CSV file automatically and fail to stage if it finds errors.

4: Select Merge or Overwrite to make changes to RSLinx Ethernet Drivers. 
    
    Merge:  Will update existing driver nodes or add new drivers and nodes that do not exist. 
            (Does not delete any drivers or nodes).
               
    Overwrite: Finds existing drivers (by name) and will overwrite all nodes with the nodes specified in the CSV. 
               Also adds new drivers that do not already exist.     

## Built Using
* C++
* Qt 6 (Widgets)
* Visual Studio 2026
* Windows Registry API



