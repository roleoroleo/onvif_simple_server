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

#ifndef PTZ_SERVICE_H
#define PTZ_SERVICE_H

int ptz_get_service_capabilities();
int ptz_get_configurations();
int ptz_get_configuration();
int ptz_get_configuration_options();
int ptz_get_nodes();
int ptz_get_node();
int ptz_get_presets();
int ptz_goto_preset();
int ptz_goto_home_position();
int ptz_continuous_move();
int ptz_relative_move();
int ptz_stop();
int ptz_get_status();
int ptz_set_preset();
int ptz_set_home_position();
int ptz_remove_preset();

int ptz_unsupported(const char *method);

#endif //PTZ_SERVICE_H