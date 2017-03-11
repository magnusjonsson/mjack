#define SYNTH_URI "urn:magnusjonsson:mjack:lv2:synth"
#define SCALA_FILE_URI SYNTH_URI "#scala_file"

struct synth_uris {
  LV2_URID midi_event;
  LV2_URID atom_urid;
  LV2_URID atom_object;
  LV2_URID atom_path;
  LV2_URID patch_get;
  LV2_URID patch_set;
  LV2_URID patch_property;
  LV2_URID patch_value;
  LV2_URID scala_file;
};

static void synth_uris_init(struct synth_uris *uris, LV2_URID_Map *map) {
  uris->midi_event = lv2_map(map, LV2_MIDI__MidiEvent);
  uris->atom_urid = lv2_map(map, LV2_ATOM__URID);
  uris->atom_object = lv2_map(map, LV2_ATOM__Object);
  uris->atom_path = lv2_map(map, LV2_ATOM__Path);
  uris->patch_get = lv2_map(map, LV2_PATCH__Get);
  uris->patch_set = lv2_map(map, LV2_PATCH__Set); 
  uris->patch_property = lv2_map(map, LV2_PATCH__property); 
  uris->patch_value = lv2_map(map, LV2_PATCH__value);
  uris->scala_file = lv2_map(map, SCALA_FILE_URI);
}

static void write_set_scala_file(LV2_Atom_Forge *forge,
				 const struct synth_uris *uris,
				 const char *filename) {
  LV2_Atom_Forge_Frame frame;
  lv2_atom_forge_object(forge, &frame, 1, uris->patch_set);
  lv2_atom_forge_property_head(forge, uris->patch_property, 0);
  lv2_atom_forge_urid(forge, uris->scala_file);
  lv2_atom_forge_property_head(forge, uris->patch_value, 0);
  lv2_atom_forge_path(forge, filename, strlen(filename));
  lv2_atom_forge_pop(forge, &frame);
}

static const char *read_set_scala_file(const LV2_Atom_Object *obj,
					   const struct synth_uris *uris) {
  if (obj->body.otype != uris->patch_set) {
   fprintf(stderr, "body type is not patch set!");
    return NULL;    
  }
  const LV2_Atom *property = NULL;
  lv2_atom_object_get(obj, uris->patch_property, &property, 0);
  if (!property) {
    fprintf(stderr, "Malformed set message has no body.\n");
    return NULL;
  }
  if (property->type != uris->atom_urid) {
    fprintf(stderr, "Malformed set message has non-URID property.\n");
    return NULL;
  }
  const LV2_Atom_URID *urid = (const LV2_Atom_URID *) property;
  if (urid->body != uris->scala_file) {
    fprintf(stderr, "Set message for unknown property.\n");
    return NULL;
  }

  const LV2_Atom *value = NULL;
  lv2_atom_object_get(obj, uris->patch_value, &value, 0);
  if (!value) {
    fprintf(stderr, "Malformed set message has no value.\n");
    return NULL;
  }
  if (value->type != uris->atom_path) {
    fprintf(stderr, "Set message value is not a Path.\n");
    return NULL;
  }

  return (const char *) (value + 1);
}
