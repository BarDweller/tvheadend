/*
 *  HDPVR Input
 *
 *  Copyright (C) 2013 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __HDPVR_PRIVATE_H__
#define __HDPVR_PRIVATE_H__

#include "input/mpegts.h"
#include "input/mpegts/hdpvr.h"
#include "htsbuf.h"

#define HDPVR_PKT_SIZE (300*188)

typedef struct hdpvr_input   hdpvr_input_t;
typedef struct hdpvr_network hdpvr_network_t;
typedef struct hdpvr_mux     hdpvr_mux_t;
typedef struct hdpvr_service hdpvr_service_t;

struct hdpvr_input
{
  mpegts_input_t;
};

struct hdpvr_network
{
  mpegts_network_t;
};

struct hdpvr_mux
{
  mpegts_mux_t;

  int                   mm_hdpvr_fd;
  mpegts_mux_instance_t mm_hdpvr_instance;
  char                 *mm_hdpvr_url;
  char                 *mm_hdpvr_interface;

  uint8_t              *mm_hdpvr_tsb;
  int                   mm_hdpvr_pos;
};

hdpvr_mux_t* hdpvr_mux_create ( const char *uuid, htsmsg_t *conf );

struct hdpvr_service
{
  mpegts_service_t;
};

hdpvr_service_t *hdpvr_service_create0
  ( hdpvr_mux_t *im, uint16_t sid, uint16_t pmt_pid,
    const char *uuid, htsmsg_t *conf );

extern hdpvr_input_t   hdpvr_input;
extern hdpvr_network_t hdpvr_network;

void hdpvr_mux_load_all     ( void );

#endif /* __HDPVR_PRIVATE_H__ */

/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
