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
#include "tvhpoll.h"
#include "tcp.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <regex.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>

/*
 * Globals
 */
hdpvr_input_t    hdpvr_input;
hdpvr_network_t  hdpvr_network;
tvhpoll_t      *hdpvr_poll;
pthread_t       hdpvr_thread;
pthread_mutex_t hdpvr_lock;

/* Handles are no longer per mux.
   When we support multiple hdpvrs,
   then all this (and the above)
   will move to an array of struct */
int      device_fd = -1;
uint8_t *device_tsb = NULL;
int      device_pos = 0;
int      device_use_count = 0;


static int hdpvr_connect (char *devicePath)
{
  int fd;
  char buf[1024];

  /* Make connection */
  tvhlog(LOG_DEBUG, "hdpvr", "connecting to hdpvr %s",
         devicePath);
  fd = tvh_open(devicePath, O_RDONLY | O_NONBLOCK, 0);
  if (fd < 0) {
    tvhlog(LOG_ERR, "hdpvr", "device open failed %s, %d (%s)", buf, errno, strerror(errno));
    return -1;
  }else{
    tvhlog(LOG_INFO, "hdpvr", "device opened ok.  %s", buf);
  }

  return fd;
}

/*
 * Input definition
 */
static const char *
hdpvr_input_class_get_title ( idnode_t *self )
{
  return "HDPVR";
}
extern const idclass_t mpegts_input_class;
const idclass_t hdpvr_input_class = {
  .ic_super      = &mpegts_input_class,
  .ic_class      = "hdpvr_input",
  .ic_caption    = "HDPVR Input",
  .ic_get_title  = hdpvr_input_class_get_title,
  .ic_properties = (const property_t[]){
    {}
  }
};

static int
hdpvr_input_start_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  int ret = SM_CODE_TUNING_FAILED;
  hdpvr_mux_t *im = (hdpvr_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_hdpvr_instance);
  char buf[256];
  //mpegts_mux_t *mm;

  im->mm_display_name((mpegts_mux_t*)im, buf, sizeof(buf));

  pthread_mutex_lock(&hdpvr_lock);
  if (!im->mm_active) {
    tvhlog(LOG_INFO, "hdpvr", "asked to start mux (%s), device fd is currently %d",buf,device_fd);
    if(device_fd==-1){
      device_fd = hdpvr_connect(im->mm_hdpvr_url);
      tvhlog(LOG_INFO, "hdpvr", "after connect, device fd is now %d",device_fd);
      /* OK */
      if (device_fd != -1) {
        tvhlog(LOG_INFO, "hdpvr", "Creating poll..");
        tvhpoll_event_t ev;
        memset(&ev, 0, sizeof(ev));
        ev.events          = TVHPOLL_IN;
        ev.fd = ev.data.fd = device_fd;
        if (tvhpoll_add(hdpvr_poll, &ev, 1) == -1) {
          tvherror("hdpvr", "%s - failed to add to poll q", buf);
          close(im->mm_hdpvr_fd);
          im->mm_hdpvr_fd = -1;
        } else {
          tvhlog(LOG_INFO, "hdpvr", "Poll created...");
          im->mm_active   = mmi;
          device_pos = 0;
          device_tsb = calloc(1, HDPVR_PKT_SIZE);
  
          /* Install table handlers */
          mpegts_table_add(mmi->mmi_mux, DVB_PAT_BASE, DVB_PAT_MASK,
                           dvb_pat_callback, NULL, "pat",
                           MT_QUICKREQ| MT_CRC, DVB_PAT_PID);
  
          ret = 0;
        }
      }
    }else{
      //we are already "tuned"
      ret = 0;
    }
    if(device_fd!=-1){
      device_use_count++;
    }
  }else{
      tvhlog(LOG_INFO, "hdpvr", "asked to start mux, but was already active. url %s interface %s",im->mm_hdpvr_url,im->mm_hdpvr_interface);
  }
  pthread_mutex_unlock(&hdpvr_lock);

  return ret;
}

static void
hdpvr_input_stop_mux ( mpegts_input_t *mi, mpegts_mux_instance_t *mmi )
{
  hdpvr_mux_t *im = (hdpvr_mux_t*)mmi->mmi_mux;
  assert(mmi == &im->mm_hdpvr_instance);
  
  tvhlog(LOG_INFO, "hdpvr", "asked ito stop mux (%s) use count is currently %d",im->mm_hdpvr_interface,device_use_count);

  pthread_mutex_lock(&hdpvr_lock);
  if (im->mm_active) {

    device_use_count--;

    if(device_use_count==0){
      
      tvhlog(LOG_INFO, "hdpvr", "use count has hit zero.. shutting it down..");

      /* Close file */
      close(device_fd); // removes from poll
      sleep(1);
      device_fd=-1;
  
      /* Free memory */
      free(device_tsb);
      device_tsb = NULL;
      device_pos = 0;
    }

    tvhlog(LOG_INFO, "hdpvr", "stopped mux (%s)",im->mm_hdpvr_interface);
  }else{
    tvhlog(LOG_INFO, "hdpvr", "did not stop, as was not active (%s)",im->mm_hdpvr_interface);
  }
  pthread_mutex_unlock(&hdpvr_lock);
}

static int
hdpvr_input_is_free ( mpegts_input_t *mi )
{

  tvhlog(LOG_INFO, "hdpvr", "asked if input is free. device_use_count is now %d",device_use_count);
  int inUse = 0;

  inUse = (device_use_count > 0) ? 1 : 0;
  
  tvhlog(LOG_INFO, "hdpvr", "is free returning %d",
             (inUse==0));

  return inUse==0;
}
/*
static int
hdpvr_input_get_weight ( mpegts_input_t *mi )
{
  return 0; // unlimited number of muxes
}
*/

static void
hdpvr_input_display_name ( mpegts_input_t *mi, char *buf, size_t len )
{
  snprintf(buf, len, "HDPVR");
}

static void *
hdpvr_input_thread ( void *aux )
{
  int nfds, fd, r;
  hdpvr_mux_t *im;
  mpegts_mux_t *mm;
  tvhpoll_event_t ev;

  while ( 1 ) {
    nfds = tvhpoll_wait(hdpvr_poll, &ev, 1, 2500);
    if ( nfds < 0 ) {
      tvhlog(LOG_ERR, "hdpvr", "poll() error %s, sleeping 1 second",
             strerror(errno));
      sleep(1);
      continue;
    } else if ( nfds == 0 ) {
      continue;
    }
    fd = ev.data.fd;

    pthread_mutex_lock(&hdpvr_lock);

    /* Read data */
    r  = read(fd, device_tsb+device_pos,
              HDPVR_PKT_SIZE-device_pos);

    /* Error */
    if (r < 0) {
      tvhlog(LOG_ERR, "hdpvr", "read() error %s", strerror(errno));
      // TODO: close and remove?
      continue;
    }
    r += device_pos;
    device_pos=-1;

    /* Play the data to each active mux */
    LIST_FOREACH(mm, &hdpvr_network.mn_muxes, mm_network_link) {
      if (((hdpvr_mux_t*)mm)->mm_active){
        im = (hdpvr_mux_t*)mm;
        im->mm_hdpvr_pos = mpegts_input_recv_packets((mpegts_input_t*)&hdpvr_input,
                                    &im->mm_hdpvr_instance,
                                    device_tsb, r, NULL, NULL, "hdpvr");
        if(device_pos==-1){
          device_pos=im->mm_hdpvr_pos;
        }else{
          if(device_pos != im->mm_hdpvr_pos){
            tvhlog(LOG_ERR,"hdpvr", "Error.. excess data did not have same value for send ts data for instance");
          }
        }
      }
    }

    pthread_mutex_unlock(&hdpvr_lock);
  }
  return NULL;
}

/*
 * Network definition
 */
extern const idclass_t mpegts_network_class;
const idclass_t hdpvr_network_class = {
  .ic_super      = &mpegts_network_class,
  .ic_class      = "hdpvr_network",
  .ic_caption    = "HDPVR Network",
  .ic_properties = (const property_t[]){
    {}
  }
};

static mpegts_mux_t *
hdpvr_network_create_mux2
  ( mpegts_network_t *mm, htsmsg_t *conf )
{
  return (mpegts_mux_t*)hdpvr_mux_create(NULL, conf);
}

static mpegts_service_t *
hdpvr_network_create_service
  ( mpegts_mux_t *mm, uint16_t sid, uint16_t pmt_pid )
{
  return (mpegts_service_t*)
    hdpvr_service_create0((hdpvr_mux_t*)mm, sid, pmt_pid, NULL, NULL);
}

static const idclass_t *
hdpvr_network_mux_class ( mpegts_network_t *mm )
{
  extern const idclass_t hdpvr_mux_class;
  return &hdpvr_mux_class;
}

/*
 * Intialise and load config
 */
void hdpvr_init ( void )
{
  pthread_t tid;
  
  /* Init Input */
  mpegts_input_create0((mpegts_input_t*)&hdpvr_input,
                       &hdpvr_input_class, NULL, NULL);
  hdpvr_input.mi_start_mux      = hdpvr_input_start_mux;
  hdpvr_input.mi_stop_mux       = hdpvr_input_stop_mux;
  hdpvr_input.mi_is_free        = hdpvr_input_is_free;
//  hdpvr_input.mi_get_weight     = hdpvr_input_get_weight;
  hdpvr_input.mi_display_name   = hdpvr_input_display_name;
  hdpvr_input.mi_enabled        = 1;

  /* Init Network */
  mpegts_network_create0((mpegts_network_t*)&hdpvr_network,
                         &hdpvr_network_class, NULL, "HDPVR Network", NULL);
  hdpvr_network.mn_create_service = hdpvr_network_create_service;
  hdpvr_network.mn_mux_class      = hdpvr_network_mux_class;
  hdpvr_network.mn_mux_create2    = hdpvr_network_create_mux2;

  /* Link */
  mpegts_input_set_network((mpegts_input_t*)&hdpvr_input,
                           (mpegts_network_t*)&hdpvr_network);
  /* Set table thread */
  tvhthread_create(&tid, NULL, mpegts_input_table_thread, &hdpvr_input, 1);

  /* Setup TS thread */
  // TODO: could set this up only when needed
  hdpvr_poll = tvhpoll_create(10);
  pthread_mutex_init(&hdpvr_lock, NULL);
  tvhthread_create(&hdpvr_thread, NULL, hdpvr_input_thread, NULL, 1);

  /* Load config */
  hdpvr_mux_load_all();
}


/******************************************************************************
 * Editor Configuration
 *
 * vim:sts=2:ts=2:sw=2:et
 *****************************************************************************/
