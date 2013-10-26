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

#include "hdpvr_private.h"
#include "settings.h"
#include "service.h"
#include "input/mpegts.h"

extern const idclass_t mpegts_service_class;

static void
hdpvr_service_config_save ( service_t *s )
{
  mpegts_service_t *ms = (mpegts_service_t*)s;
  htsmsg_t *c = htsmsg_create_map();
  service_save(s, c);
  hts_settings_save(c, "input/hdpvr/muxes/%s/services/%s",
                    idnode_uuid_as_str(&ms->s_dvb_mux->mm_id),
                    idnode_uuid_as_str(&ms->s_id));
  htsmsg_destroy(c);
}

static int
hdpvr_service_start(service_t *t, int instance){
  tvhlog(LOG_INFO,"hdpvr","Hdpvr asked to start service.. (%s) instance %d",(char*)t->s_nicename,instance);
  return mpegts_service_start(t,instance);
}

/*
 * Create
 */
hdpvr_service_t *
hdpvr_service_create0
  ( hdpvr_mux_t *im, uint16_t sid, uint16_t pmt,
    const char *uuid, htsmsg_t *conf )
{
  hdpvr_service_t *is = (hdpvr_service_t*)
    mpegts_service_create0(calloc(1, sizeof(mpegts_service_t)),
                           &mpegts_service_class, uuid,
                           (mpegts_mux_t*)im, sid, pmt, conf);
  
  is->s_config_save = hdpvr_service_config_save;
  is->s_start_feed = hdpvr_service_start;

  return is;
}
