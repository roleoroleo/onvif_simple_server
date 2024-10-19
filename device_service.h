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

#ifndef DEVICE_SERVICE_H
#define DEVICE_SERVICE_H

int device_get_services();
int device_get_service_capabilities();
int device_get_device_information();
int device_get_system_date_and_time();
int device_system_reboot();
int device_get_scopes();
int device_get_users();
int device_get_wsdl_url();
int device_get_capabilities();
int device_get_network_interfaces();
int device_get_discovery_mode();

int device_unsupported(const char *method);

#endif //DEVICE_SERVICE_H