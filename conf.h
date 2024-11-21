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

#define DEFAULT_MANUFACTURER "Manufacturer"
#define DEFAULT_MODEL        "Model"
#define DEFAULT_FW_VER       "0.0.1"
#define DEFAULT_SERIAL_NUM   "SN1234567890"
#define DEFAULT_HW_ID        "HWID"
#define DEFAULT_IFS          "wlan0"

int process_conf_file(char *file);
void free_conf_file();
void print_conf_help();
