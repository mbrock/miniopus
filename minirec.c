/*
 * This program serves as an audio capture server, capturing audio
 * input from a specified device and streaming it to connected clients
 * over UNIX sockets. Utilizing the miniaudio library for audio
 * capture and pthreads for client management, it listens for client
 * connections, handles client disconnections, and streams the
 * captured audio data to all connected clients in real time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#define SOCKET_PATH "minirec.sock"

static int server_fd, client_fds[10];
static unsigned long client_count = 0;
static pthread_mutex_t client_fds_lock;

static void 
remove_client (int client_fd) {
  for (unsigned i = 0; i < client_count; i++) 
    {
      if (client_fds[i] == client_fd) 
	{
	  close (client_fds[i]);

	  for (unsigned j = i; j < client_count - 1; j++)
	    client_fds[j] = client_fds[j + 1];

	  client_count--;
	  break;
	}
    }
}

void 
our_audio_callback (ma_device *device, 
		    void *output __attribute__((unused)), 
		    const void *input, 
		    ma_uint32 frames) 
{
  pthread_mutex_lock (&client_fds_lock);

  for (unsigned i = 0; i < client_count; i++)
    if (write (client_fds[i], 
	       input, 
	       frames * device->capture.channels * sizeof(ma_uint16)) < 0)
      {
	if (errno == EPIPE || errno == ECONNRESET) 
	  {
	    printf ("client disconnected\n");
	    remove_client (client_fds[i]);
	    i--;
	  } 
	else
	  perror ("failed to write to client");
      }

  pthread_mutex_unlock (&client_fds_lock);
}

static void * 
connection_handler (void *arg __attribute__((unused))) 
{
  struct sockaddr_un client_addr;
  socklen_t client_addr_len = sizeof (client_addr);
  
  for (;;) 
    {
      int client_fd = accept (server_fd, 
			      (struct sockaddr *) &client_addr, 
			      &client_addr_len);

      if (client_fd < 0) 
	{
	  perror ("accept failed");
	  continue;
	}
      
      pthread_mutex_lock (&client_fds_lock);
	
      if (client_count < sizeof client_fds / sizeof client_fds[0]) 
	{
	  client_fds[client_count++] = client_fd;
	  fprintf (stderr, "client connected\n");
	} 
      else 
	{
	  fprintf (stderr, "client limit reached; connection refused\n");
	  close (client_fd);
	}
      
      pthread_mutex_unlock (&client_fds_lock);
    }

    return NULL;
}

void
usage (const char *progname) 
{
  fprintf (stderr, "Usage: %s [device name]\n", progname);
}

int 
main (int argc, char **argv) 
{
  const char *desired_device_name = NULL;

  if (argc == 2 && strcmp (argv[1], "--help") == 0)
    {
      usage (argv[0]);
      return 1;
    }
  else if (argc == 2)
    {
      desired_device_name = argv[1];
    }
  else if (argc > 2)
    {
      usage (argv[0]);
      return 1;
    }

  ma_context context;
  if (ma_context_init (NULL, 0, NULL, &context) != MA_SUCCESS) {
    fprintf (stderr, "Failed to initialize context\n");
    return 1;
  }
  
  ma_device_info *pPlaybackInfos;
  ma_uint32 playbackCount;
  ma_device_info *pCaptureInfos;
  ma_uint32 captureCount;
  
  if (ma_context_get_devices (&context, &pPlaybackInfos, &playbackCount, 
			      &pCaptureInfos, &captureCount) != MA_SUCCESS) 
    {
      fprintf (stderr, "Failed to get devices\n");
      return 1;
    }
  
  ma_device_info *pDeviceInfo = NULL;

  for (ma_uint32 iDevice = 0; iDevice < captureCount; iDevice += 1) 
    {
      printf ("%d - %s\n", iDevice, pCaptureInfos[iDevice].name);
      if (desired_device_name &&
          strncmp (desired_device_name, 
		   pCaptureInfos[iDevice].name, 
		   strlen (desired_device_name)) == 0)
	{
	  printf ("Found desired device: %s\n", pCaptureInfos[iDevice].name);
	  pDeviceInfo = &pCaptureInfos[iDevice];
	  break;
	}
    }

  if (desired_device_name == NULL)
    return 0;

  if (!pDeviceInfo) 
    {
      fprintf (stderr, "Failed to find desired device\n");
      return 1;
    }

  ma_device_config deviceConfig = 
    ma_device_config_init (ma_device_type_capture);

  deviceConfig.capture.pDeviceID = &pDeviceInfo->id;
  deviceConfig.capture.format = ma_format_s16;
  deviceConfig.capture.channels = 2;
  deviceConfig.sampleRate = 48000;
  deviceConfig.dataCallback = our_audio_callback;

  ma_device device;
  if (ma_device_init (&context, &deviceConfig, &device) != MA_SUCCESS) 
    {
      fprintf (stderr, "Failed to initialize device\n");
      return 1;
    }

  signal (SIGPIPE, SIG_IGN);
  
  // Initialize UNIX socket
  server_fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror ("socket creation failed");
    return 1;
  }

  struct sockaddr_un server_addr;
  memset (&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  strcpy (server_addr.sun_path, SOCKET_PATH);
  unlink (SOCKET_PATH);
  
  if (bind (server_fd, 
	    (struct sockaddr *) &server_addr, 
	    sizeof(server_addr)) < 0) {
    perror ("bind failed");
    close (server_fd);
    return 1;
  }

  if (listen (server_fd, 5) < 0) {
    perror ("listen failed");
    close (server_fd);
    return 1;
  }

  pthread_mutex_init (&client_fds_lock, NULL);
  pthread_t thread_id;
  if (pthread_create (&thread_id, NULL, connection_handler, NULL) != 0) {
    perror ("pthread_create failed");
    close (server_fd);
    return 1;
  }

  if (ma_device_start (&device) != MA_SUCCESS) 
    {
      fprintf (stderr, "Failed to start device\n");
      ma_device_uninit (&device);
      return 1;
    }
    
  getchar();

  printf ("exiting\n");

  ma_device_uninit (&device);
  ma_context_uninit (&context);
  
  close(server_fd);
  unlink(SOCKET_PATH);
  
  for (unsigned i = 0; i < client_count; i++) {
    close (client_fds[i]);
  }
  
  pthread_mutex_destroy (&client_fds_lock);
    
  return 0;
}
