/*
 * FILE:    process.h
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2000 University College London
 * All rights reserved.
 */

char *fork_process(struct mbus *m, char *proc_name, char *ctrl_addr, pid_t *pid, char *token);
void  kill_process(pid_t proc);

