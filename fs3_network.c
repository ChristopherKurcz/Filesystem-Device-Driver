////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_netowork.c
//  Description    : This is the network implementation for the FS3 system.
//
//
//  Author         : Christopher Kurcz
//  Last Modified  : 12/10/21
//

// Includes
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>

// Project Includes
#include <fs3_network.h>
#include <fs3_driver.h>

//  Global data
    unsigned char     *fs3_network_address = NULL; // Address of FS3 server
    unsigned short     fs3_network_port = 0;       // Port of FS3 serve
    int socketHandle = -1;
    struct sockaddr_in FS3address;

// Network functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : network_fs3_syscall
// Description  : Perform a system call over the network
//
// Inputs       : cmd - the command block to send
//                ret - the returned command block
//                buf - the buffer to place received data in
// Outputs      : 0 if successful, -1 if failure

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf){

    // gets the op code bits from the command block
    uint8_t opCodeBits = getOpCodeBits(cmd);
    
    // if the op code is for mounting the disk, a new connection must be made to the server
    if(opCodeBits == FS3_OP_MOUNT){
        // sets the protocol family of the address
        FS3address.sin_family = AF_INET;

        // sets the port of the address
        if(fs3_network_port == 0){
            FS3address.sin_port = htons(FS3_DEFAULT_PORT);
        } else {
            FS3address.sin_port = htons(fs3_network_port);
        }

        // sets the ip of the address
        char *ip;
        if(fs3_network_address == NULL){
            ip = FS3_DEFAULT_IP;
        } else {
            ip = (char *) fs3_network_address;
        }

        // creates the UNIX structure for processing from the IPv4 address
        if ( inet_aton((const char *)ip, &FS3address.sin_addr) == 0 ) { 
            // error on converting
            return( -1 );
        } 

        // creates the sochet handle
        socketHandle = socket(PF_INET, SOCK_STREAM, 0); 
        if (socketHandle == -1) {
            // error on socket creation
            return( -1 );
        } 

        // connects to the server
        if ( connect(socketHandle, (const struct sockaddr *)&FS3address, sizeof(FS3address)) == -1 ) { 
            // error on socket connection
            return( -1 );
        } 

    }

    // converts the command block to network byte order
    uint64_t networkCMD = htonll64(cmd);

    // writes the command block to the server
    if (write(socketHandle, &networkCMD, sizeof(networkCMD)) != sizeof(networkCMD) ) { 
        // error writing network data
        return( -1 );
    }

    // if the op code is for a write command, the buffer will also be sent to the server
    if(opCodeBits == FS3_OP_WRSECT){
        if (write(socketHandle, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE) { 
            // error writing network data
            return( -1 );
        }
    }

    // reads the return command block from the server
    if (read(socketHandle, &networkCMD, sizeof(networkCMD)) != sizeof(networkCMD) ) { 
        // error reading
        return( -1 );
    }

    // if the op code is for a read command, the buffer will also be read from the serber
    if(opCodeBits == FS3_OP_RDSECT){
        if (read(socketHandle, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE) { 
            // error reading network data
            return( -1 );
        }
    }

    // converts the return command block back from network byte order
    networkCMD = ntohll64(networkCMD);

    // saves the returned command block to the return command block pointer
    *ret = networkCMD;

    // if the op code is for an unmount command, it closes the connection with the server
    if(opCodeBits == FS3_OP_UMOUNT){
        close(socketHandle);
        socketHandle = -1;
    }

    // Return successfully
    return (0);
}

