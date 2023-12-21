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

#ifndef ONVIF_SIMPLE_SERVER_H
#define ONVIF_SIMPLE_SERVER_H

#define DAEMON_NO_CHDIR          01 /* Don't chdir ("/") */
#define DAEMON_NO_CLOSE_FILES    02 /* Don't close all open files */
#define DAEMON_NO_REOPEN_STD_FDS 04 /* Don't reopen stdin, stdout, and stderr to /dev/null */
#define DAEMON_NO_UMASK0        010 /* Don't do a umask(0) */
#define DAEMON_MAX_CLOSE       8192 /* Max file descriptors to close if sysconf(_SC_OPEN_MAX) is indeterminate */

typedef struct {
    int enable;
    const char *username;
    const char *password;
    const char *nonce;
    const char *created;
    int  type;
} username_token_t;

typedef enum {
    JPEG,
    MPEG4,
    H264
} stream_type;

typedef struct {
    char *name;
    int  width;
    int  height;
    char *url;
    char *snapurl;
    int  type;
} stream_profile_t;

typedef struct {
    int enable;
    char *get_position;
    char *move_left;
    char *move_right;
    char *move_up;
    char *move_down;
    char *move_stop;
    char *move_preset;
    char *set_preset;
    char *set_home_position;
    char *remove_preset;
    char *jump_to_abs;
    char *jump_to_rel;
    char *get_presets;
} ptz_node_t;

typedef struct {
    char *topic;
    char *source_name;
    char *source_value;
    char *input_file;
    int is_on;
} event_t;

typedef struct {
    int   port;
    char *user;
    char *password;

    //Device Information
    char *manufacturer;
    char *model;
    char *firmware_ver;
    char *serial_num;
    char *hardware_id;

    char *ifs;

    stream_profile_t *profiles;
    int profiles_num;

    char **scopes;
    int scopes_num;

    ptz_node_t ptz_node;
    event_t *events;
    int events_enable;
    int events_num;
} service_context_t;

// returns 0 on success -1 on error
int daemonize(int flags);

#endif //ONVIF_SIMPLE_SERVER_H
