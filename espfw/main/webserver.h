
/* Builtin Webserver */

#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

/* This struct is used to provide data to us */
struct ev {
  time_t lastupd;
  time_t lastsht4xheat;
  float hum;
  float pm010;
  float pm025;
  float pm040;
  float pm100;
  float press;
  float raing;
  float temp;
};

/* Initialize and start the Webserver. */
void webserver_start(void);

#endif /* _WEBSERVER_H_ */

