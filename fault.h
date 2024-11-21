/*
 * Copyright (c) 2024 roleo.
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

#ifndef FAULT_H
#define FAULT_H

int send_empty_response(char *ns, char *method);
int send_fault(char *service, char *rec_send, char *subcode, char *subcode_ex, char *reason, char *detail);
int send_pull_messages_fault(char *timeout, char *message_limit);
int send_action_failed_fault(char *service, int code);
int send_authentication_error();

#endif //FAULT_H
