/*
 * This program functions as a client that captures audio from a UNIX
 * socket, encodes it using the Opus codec, and then streams the
 * encoded audio to standard output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <opusenc.h>

#define SAMPLE_RATE 48000
#define CHANNELS 2

static OggOpusEnc *enc = NULL;
static int sock = -1;

void 
handle_sigint (int sig) {
  if (enc != NULL) 
    {
      ope_encoder_drain (enc);
      ope_encoder_destroy (enc);
      enc = NULL;
    }

  if (sock != -1) 
    {
      close (sock);
      sock = -1;
    }

  exit(0);
}

static int 
write_callback (void *data __attribute__((unused)), 
		const unsigned char *ptr, 
		opus_int32 len) 
{
  if (fwrite (ptr, 1, len, stdout) == (unsigned) len)
    {
      fflush (stdout);
      return 0;
    }

  return -1;
}

static int 
close_callback (void *data __attribute__((unused))) {
    return 0;
}

int 
main (int argc, char **argv) 
{
  const char *socket_path = "minirec.sock";

  if (argc == 2)
    socket_path = argv[1];
  else if (argc > 2) 
    {
      fprintf (stderr, "usage: %s [minirec socket]\n", argv[0]);
      exit (EXIT_FAILURE);
    }

  sock = socket (AF_UNIX, SOCK_STREAM, 0);
  if (sock < 0) {
    perror ("socket");
    exit (EXIT_FAILURE);
  }

  struct sockaddr_un server_addr;
  memset (&server_addr, 0, sizeof (struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy (server_addr.sun_path, 
	   socket_path,
	   sizeof server_addr.sun_path - 1);

  if (connect (sock, 
	       (struct sockaddr *) &server_addr, 
	       sizeof server_addr) < 0) {
    perror ("connect");
    close (sock);
    exit (EXIT_FAILURE);
  }

  OpusEncCallbacks callbacks = { write_callback, close_callback };
  OggOpusComments *comments = ope_comments_create();
  int error;

  enc = ope_encoder_create_callbacks (&callbacks, NULL, 
				      comments, 
				      SAMPLE_RATE, 
				      CHANNELS, 0, &error);

  if (!enc) 
    {
      fprintf (stderr, "failed to create encoder: %s\n", 
	       ope_strerror(error));
      ope_comments_destroy (comments);
      close (sock);
      exit (EXIT_FAILURE);
    }

  int mux_delay = 0;
  if (ope_encoder_ctl (enc, OPE_GET_MUXING_DELAY (&mux_delay)) != OPE_OK) 
    {
      fprintf (stderr, "failed to get muxing delay\n");
      ope_encoder_destroy (enc);
      ope_comments_destroy (comments);
      close (sock);
      exit (EXIT_FAILURE);
    }

  /* fprintf (stderr, "initial muxing delay: %d samples, %2.2f s\n",  */
  /* 	   mux_delay, mux_delay / 48000.0); */

  // set the muxing delay to 0.1s, i.e. 4800 samples
  if (ope_encoder_ctl (enc, OPE_SET_MUXING_DELAY (4800)) != OPE_OK) 
    {
      fprintf (stderr, "failed to set muxing delay\n");
      ope_encoder_destroy (enc);
      ope_comments_destroy (comments);
      close (sock);
      exit (EXIT_FAILURE);
    }

  /* fprintf (stderr, "set muxing delay to 0.1s\n"); */

  // now do the decision delay

  int decision_delay = 0;
  if (ope_encoder_ctl (enc, OPE_GET_DECISION_DELAY (&decision_delay)) 
			!= OPE_OK) 
    {
      fprintf (stderr, "failed to get decision delay\n");
      ope_encoder_destroy (enc);
      ope_comments_destroy (comments);
      close (sock);
      exit (EXIT_FAILURE);
    }

  /* fprintf (stderr, "initial decision delay: %d samples, %2.2f s\n", */
  /* 	   decision_delay, decision_delay / 48000.0); */

  // set the decision delay to 0.1s, i.e. 4800 samples
  if (ope_encoder_ctl (enc, OPE_SET_DECISION_DELAY (4800)) != OPE_OK) 
    {
      fprintf (stderr, "failed to set decision delay\n");
      ope_encoder_destroy (enc);
      ope_comments_destroy (comments);
      close (sock);
      exit (EXIT_FAILURE);
    }
  
  struct sigaction act;
  memset (&act, 0, sizeof act);
  act.sa_handler = handle_sigint;
  sigaction (SIGINT, &act, NULL);
  
  opus_int16 in[960 * 2];
  ssize_t read_size;
  while ((read_size = read (sock, in, sizeof in)) > 0)
    {
      if (ope_encoder_write (enc, in, 
			     read_size / sizeof (opus_int16) / 2)
	  != OPE_OK)
	{
	  fprintf (stderr, "failed to encode frame\n");
	  break;
	}
    }

  ope_encoder_drain (enc);
  ope_encoder_destroy (enc);
  ope_comments_destroy (comments);
  close (sock);
  
  /* fprintf(stderr, "done\n"); */
  return 0;
}
