/* This is an example MEX function to demonstrate how to work with
 * asynchronous functions in shared libraries.
 *
 * Normally, we would call a function in a library and wait for its completion.
 * However, some API's require to supply a callback function, that would
 * return results in asynchronous manner.
 *
 * In this example, a separate thread is spawned, which handles library
 * initialization, callback function and termination.
 *
 * To show proof of concept, asynchronous DNS resolver library, c-ares,
 * is used in this example.
 *
 * The mex file is intended for use in two stages.
 * Stage 1: mexasync('init') - spawn separate thread and save results from callback
 * Stage 2: mexasync('fetch') - grab results saved in callback.
 *
 * There is no error checking whatsoever */
#include "mex.h"

#include <stdio.h>
#include <process.h> /* _beginthread, _endthread */
#include <string.h>

#include "ares.h"

/* Global variable accessible both in the thread and in the MEX function */
static char ip[INET6_ADDRSTRLEN];
static int gstatus = -1;

/* Process results of c-ares asynchronous resolver */
static void callback(void *arg, int status, int timeouts, struct hostent *host)
{
    /* Save first IP address and return status */
    inet_ntop(host->h_addrtype, host->h_addr_list[0], ip, sizeof(ip));
    gstatus = status;
}

/* Wait for results and process events for name resolution */
static void wait_ares(ares_channel channel)
{
    for(;;){
        struct timeval *tvp, tv;
        fd_set read_fds, write_fds;
        int nfds;
        
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        nfds = ares_fds(channel, &read_fds, &write_fds);
        if(nfds == 0){
            break;
        }
        tvp = ares_timeout(channel, NULL, &tv);
        select(nfds, &read_fds, &write_fds, NULL, tvp);
        
        /* Invoke callbacks for pending queries */
        ares_process(channel, &read_fds, &write_fds);
    }
}

/* Work with c-ares functions in a separate thread */
void CaresThreadFunc(void *nop)
{
    ares_channel channel;
    struct ares_options options;
    int optmask = 0;
    
    /* Start c-ares library */
    ares_library_init(ARES_LIB_INIT_ALL);
    ares_init_options(&channel, &options, optmask);
    
    /* Call name resolver */
    ares_gethostbyname(channel, "google.com", AF_INET, callback, NULL);
    
    /* Wait for the results and exit thread */
    wait_ares(channel);
    ares_destroy(channel);
    ares_library_cleanup();
    _endthread();
}

void mexFunction (int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
    char *cmd;
    
    /* Validate input and get first argument */
    if(nrhs!=1)
        mexErrMsgTxt("One input required.");
    if (!mxIsChar(prhs[0]))
        mexErrMsgTxt("Input must be a string.");
    cmd = mxArrayToString(prhs[0]);
    
    /* Process commands */
    if (!strcmp(cmd, "init")) {
        /* Start the asynchronous resolver thread */
        _beginthread(CaresThreadFunc, 0, NULL);
    } else if (!strcmp(cmd, "fetch")) {
        /* Fetch resolver results */
        mexPrintf("status: %d\n", gstatus);
        mexPrintf("ip: %s\n", ip);
    } else {
        mexErrMsgTxt("Unknown command.");
    }
}
