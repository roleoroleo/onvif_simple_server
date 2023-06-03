/*
 * Copyright (c) 2023 roleo.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stdarg.h>

#define MAX_LEN 1024

int cat(char *filename, int num, ...);
int scat(char *out, char *filename, int num, ...);
long get_file_size(char * filename);
int get_ip_address(char *address, char *netmask, char *name);
int get_mac_address(char *address, char *name);
int netmask2prefixlen(char *netmask);
int str_subst(char *output, char *request, char *value, char *new_value);
int find_action(char *action, int action_size, char *request);
int find_element(char *request, char *name, char *value);
char *get_element(int *size, char *request, char *name);
int html_escape(char *url, int max_len);
int hashSHA1(char* input, unsigned long inputSize, char *output, int output_size);
void *reboot_thread(void *arg);

#endif //UTILS_H