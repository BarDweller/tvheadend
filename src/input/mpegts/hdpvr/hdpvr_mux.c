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

/*
 * Class
 */
extern const idclass_t mpegts_mux_class;
extern const idclass_t mpegts_mux_instance_class;

const idclass_t hdpvr_mux_class =
{
  .ic_super      = &mpegts_mux_class,
  .ic_class      = "hdpvr_mux",
  .ic_caption    = "HDPVR Multiplex",
  .ic_properties = (const property_t[]){
    {
      .type     = PT_STR,
      .id       = "hdpvr_url",
      .name     = "URL",
      .off      = offsetof(hdpvr_mux_t, mm_hdpvr_url),
    },
    {
      .type     = PT_STR,
      .id       = "hdpvr_interface",
      .name     = "Interface",
      .off      = offsetof(hdpvr_mux_t, mm_hdpvr_interface),
    },
    {}
  }
};

static void
hdpvr_mux_config_save ( mpegts_mux_t *mm )
{
  htsmsg_t *c = htsmsg_create_map();
  mpegts_mux_save(mm, c);
  hts_settings_save(c, "input/hdpvr/muxes/%s/config",
                    idnode_uuid_as_str(&mm->mm_id));
  htsmsg_destroy(c);
}

static void
hdpvr_mux_delete ( mpegts_mux_t *mm )
{
  hts_settings_remove("input/hdpvr/muxes/%s/config",
                      idnode_uuid_as_str(&mm->mm_id));

  mpegts_mux_delete(mm);
}

static void
hdpvr_mux_display_name ( mpegts_mux_t *mm, char *buf, size_t len )
{
  hdpvr_mux_t *im = (hdpvr_mux_t*)mm;
  int urllen = strlen(im->mm_hdpvr_url);
  strncpy(buf, im->mm_hdpvr_url, len);
  if((urllen+1) < len){
    strncpy(buf+urllen, ":",len-urllen);
    urllen++;
    strncpy(buf+urllen, im->mm_hdpvr_interface, len-urllen);
  }
}

/*
 * Create
 */
hdpvr_mux_t *
hdpvr_mux_create ( const char *uuid, htsmsg_t *conf )
{
  htsmsg_t *c, *e;
  htsmsg_field_t *f;
  int i;

  /* Create Mux */
  hdpvr_mux_t *im =
    mpegts_mux_create(hdpvr_mux, uuid,
                      (mpegts_network_t*)&hdpvr_network,
                      MPEGTS_ONID_NONE, MPEGTS_TSID_NONE, conf);

  /* Callbacks */
  im->mm_display_name     = hdpvr_mux_display_name;
  im->mm_config_save      = hdpvr_mux_config_save;
  im->mm_delete           = hdpvr_mux_delete;
  im->mm_hdpvr_fd         = -1;

  /* Create Instance */
  mpegts_mux_instance_create0(&im->mm_hdpvr_instance,
                              &mpegts_mux_instance_class,
                              NULL,
                              (mpegts_input_t*)&hdpvr_input,
                              (mpegts_mux_t*)im);

  /* Services */
  c = hts_settings_load_r(1, "input/hdpvr/muxes/%s/services",
                          idnode_uuid_as_str(&im->mm_id));
  if (c) {
    HTSMSG_FOREACH(f, c) {
      if (!(e = htsmsg_field_get_map(f))) continue;
      i=0;
      //for(i=0;i<10;i++){
        (void)hdpvr_service_create0(im, i, 0, f->hmf_name, e);
      //}
    }
  }
  

  return im;
}

/*
 * Load
 */
void
hdpvr_mux_load_all ( void )
{
  htsmsg_t *s, *e;
  htsmsg_field_t *f;

  if ((s = hts_settings_load_r(1, "input/hdpvr/muxes"))) {
    HTSMSG_FOREACH(f, s) {
      if (!(e = htsmsg_get_map_by_field(f)))  continue;
      if (!(e = htsmsg_get_map(e, "config"))) continue;
      (void)hdpvr_mux_create(f->hmf_name, e);
    }
  }
}
