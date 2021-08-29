#include <stdlib.h>

#include <gtk/gtk.h>

#include "lv2/lv2plug.in/ns/ext/atom/atom.h"
#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"
#include "lv2/lv2plug.in/ns/ext/patch/patch.h"
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"
#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include "lv2utils.h"

#define SYNTH_UI_URI "urn:magnusjonsson:mjack:lv2:synth"

struct synth_ui {
  LV2_Atom_Forge forge;

  //SamplerURIs   uris;

  LV2UI_Write_Function write;
  LV2UI_Controller     controller;

  GtkWidget* box;
  GtkWidget* button;
  GtkWidget* label;
  GtkWidget* window;  /* For optional show interface. */
};


static LV2UI_Handle
instantiate(const LV2UI_Descriptor *descriptor,
	    const char *plugin_uri,
	    const char *bundle_path,
	    LV2UI_Write_Function write_function,
	    LV2UI_Controller controller,
	    LV2UI_Widget *widget,
	    const LV2_Feature *const *features) {
  struct synth_ui *ui = calloc(1, sizeof(struct synth_ui));
  ui->write = write_function;
  ui->controller = controller;
  *widget = NULL;

  LV2_URID_Map* urid_map = get_host_feature(LV2_URID__map);
  if (!urid_map) {
    fprintf(stderr, "Could not get URID map host feature\n");
    goto err;
  }


  lv2_atom_forge_init(&ui->forge, urid_map);

  ui->box = gtk_vbox_new(FALSE, 4);
  ui->label = gtk_label_new("?");
  ui->button = gtk_button_new_with_label("Load scala file");
  ui->gtk_box_pack_start(GTK_BOX(ui->box), ui->label, TRUE, TRUE, 4);
  ui->gtk_box_pack_start(GTK_BOX(ui->box), ui->button, FALSE, FALSE, 4);
  g_signal_connect(ui->button, "clicked",
		   G_CALLBACK(on_load_scala_file_clicked),
		   ui);
  // Request state from plugin
  {
    uint8_t get_buf[PATH_MAX];
    lv2_atom_forge_set_buffer(&ui->forge, get_buf, sizeof(get_buf));
    LV2_Atom_forge_Frame frame;
    LV2_Atom *msg = lv2_atom_forge_object(&ui->forge, &frame, 0,
					  ui->uris.patch_Get);
    lv2_atom_forge_pop(&ui->forge, &frame);
    ui->write(ui->controller, 0,
	      lv2_atom_total_size(msg), ui->uris.atom_eventTransfer,
	      msg);
  }
  *widget = ui->box;
  return ui;

 err:
  free(ui);
  return NULL;
}

static void cleanup(LV2UI_Handle handle) {
  struct synth_ui *ui = handle;
  gtk_widget_destroy(ui->button);
  free(ui);
}
