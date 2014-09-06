#include <lash/lash.h>
#include <stdio.h>
#include <unistd.h>

static lash_client_t* lash_client;
static lash_args_t* lash_args;

int main(int argc, char** argv) {
  lash_args = lash_extract_args(&argc, &argv);
  lash_client = lash_init(lash_args, "dummylash", LASH_Config_File, LASH_PROTOCOL_VERSION);
  if (!lash_client) {
    fprintf(stderr, "Got NULL from lash_init\n");
    goto error;
  }

  // TODO call this after jack_activate
  // lash_jack_client_name (lash_client_client, const char * name)

  while (1) {
    lash_event_t* event = lash_get_event(lash_client);
    if (event == NULL) {
      sleep(1);
      continue;
    }
    enum LASH_Event_Type type = lash_event_get_type(event);
    switch (type) {
    case LASH_Save_File:
      {
        const char* file_name = lash_event_get_string(event);
        fprintf(stderr, "Received: LASH_Save_File %s\n", file_name);
        lash_send_event(lash_client, event);
        event = NULL;
      }
      break;
    case LASH_Restore_File:
      {
        const char* file_name = lash_event_get_string(event);
        fprintf(stderr, "Received: LASH_Restore_File %s\n", file_name);
        lash_send_event(lash_client, event);
        event = NULL;
      }
      break;
    case LASH_Quit:
      fprintf(stderr, "Received: LASH_Quit\n");
      goto cleanup;
    default:
      fprintf(stderr, "LASH event type not handled: %i\n", type);
    }

    if (event != NULL) { lash_event_destroy(event); }
  }

  goto cleanup;
 error:
  fprintf(stderr, "error!\n");
 cleanup:
  if (lash_client) { }
  if (lash_args) { lash_args_destroy(lash_args); lash_args = NULL; }
  return 0;
}
