/***************************************************************************************
 *
 *  fluidsynth~
 *
 *  Fluid Synth soundfont synthesizer for Max/MSP.
 *
 *  Fluid Synth is written by Peter Hanappe et al.
 *  see http://www.fluidsynth.org/
 *
 *  Max/MSP integration by Norbert Schnell ATR IRCAM - Centre Pompidou
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *  
 *  See file COPYING.LIB for further informations on licensing terms.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
 
/************************************************************************
 *
 *  versions:
 *    (6): introduced message 'info'
 *    (5): fixed bogus path translation at file loading
 * 
 */
#define VERSION "04/2004 (6)"

#include "ftmax.h"
#include "fluidsynth.h"
#include "fluid_synth.h"
#include "fluid_sfont.h"
#include "fluid_chan.h"

/* these functions were added after the v1.0 API freeze. They are not in synth.h, yet */
extern int fluid_synth_program_select2(fluid_synth_t* synth, int chan, char* sfont_name, unsigned int bank_num, unsigned int preset_num);

typedef struct
{
	ftmax_dsp_object_t obj;
  
  fluid_synth_t *synth;
	fluid_settings_t *settings;
	int reverb;
	int chorus;
	int mute;
	void *outlet;
} fluidmax_t;

static t_messlist *fluidmax_class;

static ftmax_symbol_t sym_on = NULL;
static ftmax_symbol_t sym_off = NULL;
static ftmax_symbol_t sym_gain = NULL;
static ftmax_symbol_t sym_channels = NULL;
static ftmax_symbol_t sym_channel = NULL;
static ftmax_symbol_t sym_soundfonts = NULL;
static ftmax_symbol_t sym_soundfont = NULL;
static ftmax_symbol_t sym_presets = NULL;
static ftmax_symbol_t sym_preset = NULL;
static ftmax_symbol_t sym_reverb = NULL;
static ftmax_symbol_t sym_chorus = NULL;

/***************************************************************
 *
 *  dsp
 *
 */
static t_int *
fluidmax_perform(t_int *w)
{
  fluidmax_t *self = (fluidmax_t *)(w[1]);
  t_float *left = (t_float *)(w[2]);
  t_float *right = (t_float *)(w[3]);
  int n_tick = (int)(w[4]);

  if(self->mute == 0)
  	fluid_synth_write_float(self->synth, n_tick, left, 0, 1, right, 0, 1);
 else
 {
  int i;
  
  for(i=0; i<n_tick; i++)
    left[i] = right[i] = 0.0;
 }
	
  return (w + 5);
}

static void 
fluidmax_dsp(fluidmax_t *self, t_signal **sp, short *count)
{
  double sr = sp[0]->s_sr;
  int n_tick = sp[0]->s_n;
        
  dsp_add(fluidmax_perform, 4, self, sp[0]->s_vec, sp[1]->s_vec, n_tick);
}

/***************************************************************
 *
 *  load utlilities
 *
 */
static char *
fluidmax_translate_fullpath(char *maxpath, char *fullpath)
{
	int i;

  strcpy(fullpath, "/Volumes/");
  
	for(i=0; maxpath[i] != ':'; i++)
	  fullpath[i + 9] = maxpath[i];
	  
	/* skip ':' */
	i++;
	  
  strcpy(fullpath + i + 8, maxpath + i);
	
	return fullpath;
}
  	
static ftmax_symbol_t
fluidmax_get_stripped_name(const char *fullpath)
{
  char stripped[1024];
	int i;
  
	for(i=strlen(fullpath)-1; i>=0; i--)
	{
	  if(fullpath[i] == '/')
      break;
  }
  
  if(i != 0)
    i++;	  
	
	strcpy(stripped, fullpath + i);
	
	for(i=0; stripped[i] != '\0'; i++)
  {
    if((stripped[i] == '.') && 
       (stripped[i + 1] == 's' || stripped[i + 1] == 'S') && 
       (stripped[i + 2] == 'f' || stripped[i + 2] == 'F') && 
       (stripped[i + 3] == '2'))
    {
      stripped[i] = '\0';
      break;
    }
  }
	
	return ftmax_new_symbol(stripped);
}
  	
static ftmax_symbol_t
fluidmax_sfont_get_name(fluid_sfont_t *sfont)
{
  return fluidmax_get_stripped_name(fluid_sfont_get_name(sfont));
}
  	
static fluid_sfont_t *
fluidmax_sfont_get_by_name(fluidmax_t *self, ftmax_symbol_t name)
{
  int n = fluid_synth_sfcount(self->synth);
  int i;
  
  for(i=0; i<n; i++)
  {
    fluid_sfont_t *sfont = fluid_synth_get_sfont(self->synth, i);

    if(fluidmax_sfont_get_name(sfont) == name)
      return sfont;
  }

	return NULL;
}

static void 
fluidmax_do_load(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
	if(ac > 0 && ftmax_is_symbol(at))	
	{
		const char *filename = ftmax_symbol_name(ftmax_get_symbol(at));
		ftmax_symbol_t name = fluidmax_get_stripped_name(filename);
		fluid_sfont_t *sf = fluidmax_sfont_get_by_name(self, name);
		
		if(sf == NULL)
		{
		  int id = fluid_synth_sfload(self->synth, filename, 0);
		
    	if(id >= 0)
    	{
    		post("fluidsynth~: loaded soundfont '%s'", ftmax_symbol_name(name));

    	  fluid_synth_program_reset(self->synth);
    	  
    	  outlet_bang(self->outlet);
      }
      else
    		error("fluidsynth~: cannot load soundfont from file '%s'", filename);
    }
    else
    {
		  error("soundfont named '%s' is already loaded", ftmax_symbol_name(name));
		  return;
		}
  }
}

static void
fluidmax_load_with_dialog(t_object *o, t_symbol *s, short argc, t_atom *argv)
{
	char filename[256];
	char maxpath[1024];
	char fullpath[1024];
  long type;
	short path;
	
	open_promptset("open SoundFont 2 file");
	
	if(open_dialog(filename, &path, &type, 0, 0))
		return;
		
	if(path_topotentialname(path, filename, maxpath, 0) == 0)
	{
    ftmax_atom_t a;
    
  	ftmax_set_symbol(&a, ftmax_new_symbol(fluidmax_translate_fullpath(maxpath, fullpath)));
    fluidmax_do_load(o, NULL, 1, &a);
  }
}	

/***************************************************************
 *
 *  user methods
 *
 */
static void 
fluidmax_load(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac == 0)
	  defer(o, (method)fluidmax_load_with_dialog, NULL, 0, 0);
	else
	{
	  if(ftmax_is_symbol(at))
	  {
	    ftmax_symbol_t name = ftmax_get_symbol(at);
	    char *string = (char *)ftmax_symbol_name(name);
	    
	    if(string[0] == '/')
    	  defer(o, (method)fluidmax_do_load, NULL, ac, at);
      else
	    {
      	char maxpath[1024];
      	char fullpath[1024];
	      short path;
      	long type;
        ftmax_atom_t a;
        
        if(locatefile_extended(string, &path, &type, 0, 0) || path_topotentialname(path, string, maxpath, 0) != 0)
      	{
      	  error("fluidsynth~: cannot find file '%s'", string);
      	  return;
      	}
      	
	      ftmax_set_symbol(&a, ftmax_new_symbol(fluidmax_translate_fullpath(maxpath, fullpath)));
    	  defer(o, (method)fluidmax_do_load, NULL, 1, &a);
	    }
    }
  }
}

static void 
fluidmax_unload(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
	if(ac > 0)
	{
	  if(ftmax_is_number(at))	
  	{
  		int id = ftmax_get_number_int(at);
      fluid_sfont_t *sf = fluid_synth_get_sfont_by_id(self->synth, id);
      
      if(sf != NULL)
      {  
        ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
        
    		if(fluid_synth_sfunload(self->synth, id, 0) >= 0)
    		{
    			post("fluidsynth~: unloaded soundfont '%s' (%d)", ftmax_symbol_name(name), id);
    			return;
    	  }
      }
        	  
  	  error("fluidsynth~: cannot unload soundfont %d", id);
  	}
    else if (ftmax_is_symbol(at))
    {
      ftmax_symbol_t sym = ftmax_get_symbol(at);
      
      if(sym == ftmax_new_symbol("all"))
      {
        fluid_sfont_t *sf = fluid_synth_get_sfont(self->synth, 0);
        int i;
        
        fluid_synth_system_reset(self->synth);

        while(sf != NULL)
        {
          ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
          unsigned int id = fluid_sfont_get_id(sf);
        
      		if(fluid_synth_sfunload(self->synth, id, 0) >= 0)
      			post("fluidsynth~: unloaded soundfont '%s' (%d)", ftmax_symbol_name(name), id);
      	  else
        	  error("fluidsynth~: cannot unload soundfont '%s' (%d)", ftmax_symbol_name(name), id);
        
          sf = fluid_synth_get_sfont(self->synth, 0);
        }
      }
      else
      {
        fluid_sfont_t *sf = fluidmax_sfont_get_by_name(self, sym);
        
        if(sf != NULL)
        {
          unsigned int id = fluid_sfont_get_id(sf);
          
      		if(fluid_synth_sfunload(self->synth, id, 0) >= 0)
      		{
      			post("fluidsynth~: unloaded soundfont '%s' (%d)", ftmax_symbol_name(sym), id);
      			return;
      	  }
      	}
      	
    	  error("fluidsynth~: cannot unload soundfont '%s'", ftmax_symbol_name(sym));
      }
    }
  }
}

static void 
fluidmax_reload(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
	if(ac > 0)
	{
	  int id;
	
  	if(ftmax_is_number(at))	
  	{
  		int id = ftmax_get_number_int(at);
      fluid_sfont_t *sf = fluid_synth_get_sfont_by_id(self->synth, id);
  		
  		if(sf != NULL)
  		{
      	ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
      
  		  if (fluid_synth_sfreload(self->synth, id) >= 0)
  		  {
  			  post("fluidsynth~: reloaded soundfont '%s' (%d)", id);
  			  return;
  			}

  			error("fluidsynth~: cannot reload soundfont %d", id);
    	}
  	}
    else if (ftmax_is_symbol(at))
    {
      ftmax_symbol_t sym = ftmax_get_symbol(at);
      
      if(sym == ftmax_new_symbol("all"))
      {
        int n = fluid_synth_sfcount(self->synth);
        int i;
        
        fluid_synth_system_reset(self->synth);

        for(i=0; i<n; i++)
        {
          fluid_sfont_t *sf = fluid_synth_get_sfont(self->synth, i);
          ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
          unsigned int id = fluid_sfont_get_id(sf);
          
        
  		  if (fluid_synth_sfreload(self->synth, id) >= 0)
      			post("fluidsynth~: reloaded soundfont '%s' (%d)", ftmax_symbol_name(name), id);
      	  else
        	  error("fluidsynth~: cannot reload soundfont '%s' (%d)", ftmax_symbol_name(name), id);
        }
      }
      else
      {
        fluid_sfont_t *sf = fluidmax_sfont_get_by_name(self, sym);
        
        if(sf != NULL)
        {
          unsigned int id = fluid_sfont_get_id(sf);
          
      		if(fluid_synth_sfreload(self->synth, id) >= 0)
      		{
      			post("fluidsynth~: reloaded soundfont '%s' (%d)", ftmax_symbol_name(sym), id);
      			return;
      	  }
      	}
      	
    	  error("fluidsynth~: cannot reload soundfont '%s'", ftmax_symbol_name(sym));
      }
    }
  }
}

static void 
fluidmax_note(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int velocity = 64;
    int channel = 1;
    
    switch(ac)
    {
      default:
      case 3:
        if(ftmax_is_number(at + 2))
        {
          channel = ftmax_get_number_int(at + 2);
                    
          if(channel < 1)
            channel = 1;
          else if(channel > fluid_synth_count_midi_channels(self->synth))
            channel = fluid_synth_count_midi_channels(self->synth);
        }
      case 2:
        if(ftmax_is_number(at + 1))
          velocity = ftmax_get_number_int(at + 1);
      case 1:
		    fluid_synth_noteon(self->synth, channel - 1, ftmax_get_number_int(at), velocity);
  		case 0:
  		  break;
  	}
  }
}

static void 
fluidmax_list(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_note(o, NULL, ac, at);
}


static void 
fluidmax_control_change(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int value = 64;
    int channel = 1;
    
    switch(ac)
    {
      default:
      case 3:
        if(ftmax_is_number(at + 2))
        {
          channel = ftmax_get_number_int(at + 2);
          
          if(channel < 1)
            channel = 1;
          else if(channel > fluid_synth_count_midi_channels(self->synth))
            channel = fluid_synth_count_midi_channels(self->synth);
        }
      case 2:
        if(ftmax_is_number(at + 1))
          value = ftmax_get_number_int(at + 1);
      case 1:
		    fluid_synth_cc(self->synth, channel - 1, ftmax_get_number_int(at), value);
  		case 0:
  		  break;
  	}
  }
}

static void 
fluidmax_mod(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 1 && ftmax_is_number(at) && ftmax_is_number(at + 1))
  {
    int param = ftmax_get_number_int(at);
    float value = ftmax_get_number_float(at + 1);
    int channel = 1;
    
    if(ac > 2 && ftmax_is_number(at + 2))
    {
      channel = ftmax_get_number_int(at + 2);
      
      if(channel < 1)
        channel = 1;
      else if(channel > fluid_synth_count_midi_channels(self->synth))
        channel = fluid_synth_count_midi_channels(self->synth);
    }

    fluid_synth_set_gen(self->synth, channel - 1, param, value);
  }
}

static void 
fluidmax_pitch_bend(t_object *o, Symbol *s, short ac, Atom *at)
{	
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int channel = 1;
    double bend = 0.0;
    
  	if(ac > 1 && ftmax_is_number(at + 1))
    {
      channel = ftmax_get_number_int(at + 1);
          
      if(channel < 1)
        channel = 1;
      else if(channel > fluid_synth_count_midi_channels(self->synth))
        channel = fluid_synth_count_midi_channels(self->synth);
    }
  	  
  	bend = ftmax_get_number_float(at);
  	
    if(bend < 0.0)
      bend = 0.0;
    else if(bend > 127.0)
      bend = 127.0;
	
		fluid_synth_pitch_bend(self->synth, channel - 1, (int)(bend * 128.0));
  }
}

static void 
fluidmax_pitch_bend_wheel(t_object *o, Symbol *s, short ac, Atom *at)
{	
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int channel = 1;
    
  	if(ac > 1 && ftmax_is_number(at + 1))
      channel = ftmax_get_number_int(at + 1);
  	  
		fluid_synth_pitch_wheel_sens(self->synth, channel - 1, ftmax_get_number_int(at));
  }
}

static void 
fluidmax_program_change(t_object *o, Symbol *s, short ac, Atom *at)
{	
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int channel = 1;
    
  	if(ac > 1 && ftmax_is_number(at + 1))
  	{
      channel = ftmax_get_number_int(at + 1);
          
      if(channel < 1)
        channel = 1;
      else if(channel > fluid_synth_count_midi_channels(self->synth))
        channel = fluid_synth_count_midi_channels(self->synth);
    }
  	  
		fluid_synth_program_change(self->synth, channel - 1, ftmax_get_number_int(at));
  }
}

static void 
fluidmax_bank_select(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0 && ftmax_is_number(at))
  {
    int channel = 1;
    
  	if(ac > 1 && ftmax_is_number(at + 1))
  	{
      channel = ftmax_get_number_int(at + 1);
          
      if(channel < 1)
        channel = 1;
      else if(channel > fluid_synth_count_midi_channels(self->synth))
        channel = fluid_synth_count_midi_channels(self->synth);
    }
  	  
		fluid_synth_bank_select(self->synth, channel - 1, ftmax_get_number_int(at));
  }
}

static void 
fluidmax_select(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  unsigned int sfont = 0;
  unsigned int bank = 0;
  unsigned int preset = 0;  
  int channel = 1;
  
  switch(ac)
  {
    default:
    case 4:
      if(ftmax_is_number(at + 3))
        channel = ftmax_get_number_int(at + 3);

      if(channel < 1)
        channel = 1;
      else if(channel > fluid_synth_count_midi_channels(self->synth))
        channel = fluid_synth_count_midi_channels(self->synth);
        
    case 3:
      if(ftmax_is_number(at + 2))
        preset = ftmax_get_number_int(at + 2);
        
    case 2:
      if(ftmax_is_number(at + 1))
        bank = ftmax_get_number_int(at + 1);
        
    case 1:
      if(ftmax_is_number(at))
    		fluid_synth_program_select(self->synth, channel - 1, ftmax_get_number_int(at), bank, preset);
      else if(ftmax_is_symbol(at))
    		fluid_synth_program_select2(self->synth, channel - 1, ftmax_symbol_name(ftmax_get_symbol(at)), bank, preset);
    		
		case 0:
		  break;
	}
}

static void 
fluidmax_reverb(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac == 0)
  {
    fluid_synth_set_reverb_on(self->synth, 1);
    fluid_revmodel_reset(self->synth->reverb);
    self->reverb = 1;
  }
  else if(ftmax_is_number(at))
  {
    double room = fluid_synth_get_reverb_roomsize(self->synth);
    double damping = fluid_synth_get_reverb_damp(self->synth);
    double width = fluid_synth_get_reverb_width(self->synth);

    fluid_synth_set_reverb_on(self->synth, 1);
    self->reverb = 1;
    
    switch(ac)
    {
      default:
      case 4:
        if(ftmax_is_number(at + 3))
          width = ftmax_get_number_float(at + 3);
      case 3:
        if(ftmax_is_number(at + 2))
          damping = ftmax_get_number_float(at + 2);
      case 2:
        if(ftmax_is_number(at + 1))
          room = ftmax_get_number_float(at + 1);
      case 1:
        fluid_synth_set_reverb(self->synth, room, damping, width, ftmax_get_number_float(at));
  		case 0:
  		  break;
  	}
  }
	else if(ftmax_is_symbol(at))
	{
	  ftmax_symbol_t sym = ftmax_get_symbol(at);
	  
	  if(sym == sym_on)
	  {
      fluid_synth_set_reverb_on(self->synth, 1);
      self->reverb = 1;
    }
    else if(sym == sym_off)
    {
      fluid_synth_set_reverb_on(self->synth, 0);
      self->reverb = 0;
    }
  }
}

static void 
fluidmax_chorus(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac == 0)
  {
    fluid_synth_set_chorus_on(self->synth, 1);
    fluid_chorus_reset(self->synth->chorus);
    self->chorus = 1;
  }
  else if(ftmax_is_number(at))
  {
    double speed = fluid_synth_get_chorus_speed_Hz(self->synth);
    double depth = fluid_synth_get_chorus_depth_ms(self->synth);
    int type = fluid_synth_get_chorus_type(self->synth);
    int nr = fluid_synth_get_chorus_nr(self->synth);

    fluid_synth_set_chorus_on(self->synth, 1);
    self->chorus = 1;

    switch(ac)
    {
      default:
      case 5:
        if(ftmax_is_number(at + 4))
          nr = ftmax_get_number_int(at + 4);
      case 4:
        if(ftmax_is_number(at + 3))
          type = ftmax_get_number_int(at + 3);
      case 3:
        if(ftmax_is_number(at + 2))
          depth = ftmax_get_number_float(at + 2);
      case 2:
        if(ftmax_is_number(at + 1))
          speed = ftmax_get_number_float(at + 1);
      case 1:
        fluid_synth_set_chorus(self->synth, nr, ftmax_get_number_float(at), speed, depth, type);
  		case 0:
  		  break;
  	}
  }
	else if(ftmax_is_symbol(at))
	{
	  ftmax_symbol_t sym = ftmax_get_symbol(at);
	  
	  if(sym == sym_on)
	  {
      fluid_synth_set_chorus_on(self->synth, 1);
      self->chorus = 1;
    }
    else if(sym == sym_off)
    {
      fluid_synth_set_chorus_on(self->synth, 0);
      self->chorus = 0;
    }
  }
}

static void 
fluidmax_gain(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac >0 && ftmax_is_number(at))
  {
    float gain = ftmax_get_number_float(at);
    
    fluid_synth_set_gain(self->synth, gain);
  }
}

static void 
fluidmax_panic(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;

  fluid_synth_system_reset(self->synth);
}

static void 
fluidmax_reset(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  int n = fluid_synth_count_midi_channels(self->synth);
  int i;

  for (i=0; i<n; i++)
    fluid_channel_reset(self->synth->channel[i]);
}

static void 
fluidmax_mute(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  int mute = 1;
  int i;
  
  if(ac > 0 && ftmax_is_number(at))
    mute = (ftmax_get_number_int(at) != 0);
  
  fluid_synth_system_reset(self->synth);
   
  self->mute = mute;
}

static void 
fluidmax_unmute(t_object *o)
{
  fluidmax_t *self = (fluidmax_t *)o;
  ftmax_atom_t a;
  
  ftmax_set_int(&a, 0);  
  fluidmax_mute(o, NULL, 1, &a);
}

/* 
int fluid_synth_count_audio_channels (fluid_synth_t *synth)
int fluid_synth_count_audio_groups (fluid_synth_t *synth)
int fluid_synth_count_effects_channels (fluid_synth_t *synth)
*/

/* tuning
fluid_synth_create_key_tuning (fluid_synth_t *synth, int tuning_bank, int tuning_prog, char *name, double *pitch)
fluid_synth_create_octave_tuning (fluid_synth_t *synth, int tuning_bank, int tuning_prog, char *name, double *pitch)
fluid_synth_tune_notes (fluid_synth_t *synth, int tuning_bank, int tuning_prog, int len, int *keys, double *pitch, int apply)
fluid_synth_select_tuning (fluid_synth_t *synth, int chan, int tuning_bank, int tuning_prog)
fluid_synth_reset_tuning (fluid_synth_t *synth, int chan)
fluid_synth_tuning_iteration_start (fluid_synth_t *synth)
fluid_synth_tuning_iteration_next (fluid_synth_t *synth, int *bank, int *prog)
fluid_synth_tuning_dump (fluid_synth_t *synth, int bank, int prog, char *name, int len, double *pitch)
*/

static void
fluidmax_version(t_object *o)
{
  post("fluidsynth~ (Fluid Synth soundfont synthesizer for Max/MSP), version  %s", VERSION);
  post("  Fluid Synth is written by Peter Hanappe et al. (see http://www.fluidsynth.org/)");
  post("  Max/MSP integration by Norbert Schnell ATR IRCAM - Centre Pompidou");
}

static void 
fluidmax_print(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0)
  {
    if(ftmax_is_symbol(at))
    {
      ftmax_symbol_t sym = ftmax_get_symbol(at);
      
      if(sym == sym_gain)
      {
        double gain = fluid_synth_get_gain(self->synth);

        post("gain: %g", gain);
      }
      else if(sym == sym_channels)
      {
        int n = fluid_synth_count_midi_channels(self->synth);
        int i;
        
        post("fluidsynth~ MIDI channels:");
        
        for(i=0; i<n; i++)
        {
          fluid_preset_t *preset = fluid_synth_get_channel_preset(self->synth, i);
          char *preset_str = fluid_preset_get_name(preset);
          ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
          unsigned int sf_id;
          unsigned int bank_num;
          unsigned int preset_num;
          fluid_sfont_t *sf;
          
          fluid_synth_get_program(self->synth, i, &sf_id, &bank_num, &preset_num);
          sf = fluid_synth_get_sfont_by_id(self->synth, sf_id);
          
          post("  channel %d: soundfont '%s', bank %d, preset %d: '%s'", 
            i + 1, ftmax_symbol_name(fluidmax_sfont_get_name(sf)), bank_num, preset_num, ftmax_symbol_name(preset_name));
          
        }
      }
      else if(sym == sym_soundfonts)
      {
        int n = fluid_synth_sfcount(self->synth);
        int i;
        
        if(n > 0)
          post("fluidsynth~ soundfonts:");
        else
          post("fluidsynth~: no soundfonts loaded");          
        
        for(i=0; i<n; i++)
        {
          fluid_sfont_t *sf = fluid_synth_get_sfont(self->synth, i);
          ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
          unsigned int id = fluid_sfont_get_id(sf);
          
          post("  soundfont %d '%s' (%d)", i, ftmax_symbol_name(name), id);
        }
      }
      else if(sym == sym_presets)
      {
        int n = fluid_synth_sfcount(self->synth);
        
        if(n > 0)
        {
          if(ac > 1)
          {
            fluid_sfont_t *sf = NULL;
            ftmax_symbol_t name;
         
            if(ftmax_is_symbol(at + 1))
            {
              name = ftmax_get_symbol(at + 1);
              sf = fluidmax_sfont_get_by_name(self, name);
            }
            else if(ftmax_is_int(at + 1))
            {
              int id = ftmax_get_int(at + 1);
              
              sf = fluid_synth_get_sfont_by_id(self->synth, id);
              name = fluidmax_sfont_get_name(sf);
            }
            
            if(sf != NULL)
            {
              fluid_preset_t preset;
              
              fluid_sfont_iteration_start(sf);
              
              post("fluidsynth~ presets of soundfont '%s':", ftmax_symbol_name(name));
              
              while(fluid_sfont_iteration_next(sf, &preset) > 0)
              {
                char *preset_str = fluid_preset_get_name(&preset);
                ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
                int bank_num = fluid_preset_get_banknum(&preset);
                int preset_num = fluid_preset_get_num(&preset);
                
                post("  preset '%s': bank %d, program %d)", ftmax_symbol_name(preset_name), bank_num, preset_num);
              }
            }
          }
          else
          {
            int i;

            post("fluidsynth~ presets:");
            
            for(i=0; i<128; i++)
            {
              int j;
              
              for(j=0; j<128; j++)
              {
                fluid_preset_t *preset = NULL;
                fluid_sfont_t *sf = NULL;
                int k;
                
                for(k=0; k<n; k++)
                {
                  sf = fluid_synth_get_sfont(self->synth, k);                  
                  preset = fluid_sfont_get_preset(sf, i, j);
                  
                  if(preset != NULL)
                    break;
                }
                
                if(preset != NULL)
                {
                  ftmax_symbol_t sf_name = fluidmax_sfont_get_name(sf);
                  char *preset_str = fluid_preset_get_name(preset);
                  ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
                  
                  post("  preset '%s': soundfont '%s', bank %d, program %d)", 
                    ftmax_symbol_name(preset_name), ftmax_symbol_name(sf_name), i, j);
                }
              }
            }
          }
        }
        else
          error("fluidsynth~: no soundfonts loaded");
      }
      else if(sym == ftmax_new_symbol("modulators"))
      {
        int channel = 1;
        int n = GEN_LAST;
        int i;
        
        if(ac > 1 && ftmax_is_number(at + 1))
          channel = ftmax_get_number_int(at + 1);

        if(channel < 1)
          channel = 1;
        else if(channel > fluid_synth_count_midi_channels(self->synth))
          channel = fluid_synth_count_midi_channels(self->synth);
          
        post("fluidsynth~ modulator states of channel %d:", channel);
        
        for(i=0; i<n; i++)
          post("  %d: %f", i, fluid_synth_get_gen(self->synth, channel - 1, i));
      }
      else if(sym == sym_reverb)
      {
        double level = fluid_synth_get_reverb_level(self->synth);
        double room = fluid_synth_get_reverb_roomsize(self->synth);
        double damping = fluid_synth_get_reverb_damp(self->synth);
        double width = fluid_synth_get_reverb_width(self->synth);
        
        if(self->reverb != 0)
        {
          post("fluidsynth~ current reverb parameters:");
          post("  level: %f", level);
          post("  room size: %f", room);
          post("  damping: %f", damping);
          post("  width: %f", width);
        }
        else
          post("fluidsynth~ reverb off");        
      }
      else if(sym == sym_chorus)
      {
        if(self->chorus != 0)
        {
          double level = fluid_synth_get_chorus_level(self->synth);
          double speed = fluid_synth_get_chorus_speed_Hz(self->synth);
          double depth = fluid_synth_get_chorus_depth_ms(self->synth);
          int type = fluid_synth_get_chorus_type(self->synth);
          int nr = fluid_synth_get_chorus_nr(self->synth);
          
          post("fluidsynth~ current chorus parameters:");
          post("  level: %f", level);
          post("  speed: %f Hz", speed);
          post("  depth: %f msec", depth);
          post("  type: %d (%s)", type, type? "triangle": "sine");
          post("  %d units", nr);
        }
        else
          post("fluidsynth~ chorus off");
      }
    }
  }
}

static void 
fluidmax_info(t_object *o, Symbol *s, short ac, Atom *at)
{
  fluidmax_t *self = (fluidmax_t *)o;
  
  if(ac > 0)
  {
    if(ftmax_is_symbol(at))
    {
      ftmax_symbol_t sym = ftmax_get_symbol(at);
      
      if(sym == sym_gain)
      {
        ftmax_atom_t a;
        double gain = fluid_synth_get_gain(self->synth);

        ftmax_set_float(&a, gain);
        outlet_anything(self->outlet, sym_channel, 1, &a);
      }
      else if(sym == sym_channels)
      {
        int n = fluid_synth_count_midi_channels(self->synth);
        int i;
        
        for(i=0; i<n; i++)
        {
          fluid_preset_t *preset = fluid_synth_get_channel_preset(self->synth, i);
          char *preset_str = fluid_preset_get_name(preset);
          ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
          unsigned int sf_id, bank_num, preset_num;
          fluid_sfont_t *sf;
          ftmax_atom_t a[5];
          
          fluid_synth_get_program(self->synth, i, &sf_id, &bank_num, &preset_num);
          sf = fluid_synth_get_sfont_by_id(self->synth, sf_id);

          ftmax_set_int(a, i + 1);
          ftmax_set_symbol(a + 1, fluidmax_sfont_get_name(sf));
          ftmax_set_int(a + 2, bank_num);
          ftmax_set_int(a + 3, preset_num);
          ftmax_set_symbol(a + 4, preset_name);
          outlet_anything(self->outlet, sym_channel, 5, a);          
        }
      }
      else if(sym == sym_soundfonts)
      {
        int n = fluid_synth_sfcount(self->synth);
        int i;
        
        for(i=0; i<n; i++)
        {
          fluid_sfont_t *sf = fluid_synth_get_sfont(self->synth, i);
          ftmax_symbol_t name = fluidmax_sfont_get_name(sf);
          unsigned int id = fluid_sfont_get_id(sf);
          ftmax_atom_t a[2];
          
          ftmax_set_int(a, i);
          ftmax_set_symbol(a + 1, fluidmax_sfont_get_name(sf));
          outlet_anything(self->outlet, sym_soundfont, 2, a);
        }
      }
      else if(sym == sym_presets)
      {
        int n = fluid_synth_sfcount(self->synth);
        
        if(n > 0)
        {
          if(ac > 1)
          {
            fluid_sfont_t *sf = NULL;
            ftmax_symbol_t sf_name;
         
            if(ftmax_is_symbol(at + 1))
            {
              sf_name = ftmax_get_symbol(at + 1);
              sf = fluidmax_sfont_get_by_name(self, sf_name);
            }
            else if(ftmax_is_int(at + 1))
            {
              int id = ftmax_get_int(at + 1);
              
              sf = fluid_synth_get_sfont_by_id(self->synth, id);
              sf_name = fluidmax_sfont_get_name(sf);
            }
            
            if(sf != NULL)
            {
              fluid_preset_t preset;
              
              fluid_sfont_iteration_start(sf);
              
              while(fluid_sfont_iteration_next(sf, &preset) > 0)
              {
                char *preset_str = fluid_preset_get_name(&preset);
                ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
                int bank_num = fluid_preset_get_banknum(&preset);
                int preset_num = fluid_preset_get_num(&preset);
                ftmax_atom_t a[4];
                
                ftmax_set_symbol(a , preset_name);
                ftmax_set_symbol(a + 1, sf_name);
                ftmax_set_int(a + 2, bank_num);
                ftmax_set_int(a + 3, preset_num);
                outlet_anything(self->outlet, sym_preset, 4, a);
              }
            }
          }
          else
          {
            int i;

            for(i=0; i<128; i++)
            {
              int j;
              
              for(j=0; j<128; j++)
              {
                fluid_preset_t *preset = NULL;
                fluid_sfont_t *sf = NULL;
                int k;
                
                for(k=0; k<n; k++)
                {
                  sf = fluid_synth_get_sfont(self->synth, k);                  
                  preset = fluid_sfont_get_preset(sf, i, j);
                  
                  if(preset != NULL)
                    break;
                }
                
                if(preset != NULL)
                {
                  ftmax_symbol_t sf_name = fluidmax_sfont_get_name(sf);
                  char *preset_str = fluid_preset_get_name(preset);
                  ftmax_symbol_t preset_name = ftmax_new_symbol(preset_str);
                  ftmax_atom_t a[4];
                  
                  ftmax_set_symbol(a , preset_name);
                  ftmax_set_symbol(a + 1, sf_name);
                  ftmax_set_int(a + 2, i);
                  ftmax_set_int(a + 3, j);
                  outlet_anything(self->outlet, sym_preset, 4, a);
                }
              }
            }
          }
        }
        else
          error("fluidsynth~ info: no soundfonts loaded");
      }
      else if(sym == sym_reverb)
      {
        if(self->reverb != 0)
        {
          double level = fluid_synth_get_reverb_level(self->synth);
          double room = fluid_synth_get_reverb_roomsize(self->synth);
          double damping = fluid_synth_get_reverb_damp(self->synth);
          double width = fluid_synth_get_reverb_width(self->synth);
          ftmax_atom_t a[4];
          
          ftmax_set_float(a, level);
          ftmax_set_float(a + 1, room);
          ftmax_set_float(a + 2, damping);
          ftmax_set_float(a + 3, width);
          outlet_anything(self->outlet, sym_reverb, 4, a);          
        }
        else
        {
          ftmax_atom_t a;
          
          ftmax_set_symbol(&a, sym_off);
          outlet_anything(self->outlet, sym_reverb, 1, &a);
        }
      }
      else if(sym == sym_chorus)
      {
        if(self->chorus != 0)
        {
          double level = fluid_synth_get_chorus_level(self->synth);
          double speed = fluid_synth_get_chorus_speed_Hz(self->synth);
          double depth = fluid_synth_get_chorus_depth_ms(self->synth);
          int type = fluid_synth_get_chorus_type(self->synth);
          int nr = fluid_synth_get_chorus_nr(self->synth);
          ftmax_atom_t a[5];
          
          ftmax_set_float(a, level);
          ftmax_set_float(a + 1, speed);
          ftmax_set_float(a + 2, depth);
          ftmax_set_int(a + 3, type);
          ftmax_set_int(a + 4, nr);
          outlet_anything(self->outlet, sym_chorus, 5, a);
        }
        else
        {
          ftmax_atom_t a;
          
          ftmax_set_symbol(&a, sym_off);
          outlet_anything(self->outlet, sym_chorus, 1, &a);
        }
      }
    }
  }
}

/***************************************************************
 *
 *  class
 *
 */
static void *
fluidmax_new(Symbol *s, short ac, Atom *at)
{  
  fluidmax_t *self = (fluidmax_t *)newobject(fluidmax_class);
  int polyphony = 256;
  int midi_channels = 16;

  self->outlet = outlet_new(self, "anything");

  dsp_setup((t_pxobject *)self, 0);
  outlet_new(self, "signal");
  outlet_new(self, "signal");

  self->synth = NULL;
	self->settings = new_fluid_settings();
  self->reverb = 0;
  self->chorus = 0;
  self->mute = 0;
	
	if(ac > 0 && ftmax_is_number(at))
	{
	  polyphony = ftmax_get_number_int(at);
	  ac--;
	  at++;
	}
	
	if(ac > 0 && ftmax_is_number(at))
	{
	  midi_channels = ftmax_get_number_int(at);
	  ac--;
	  at++;
	}
	
	if(ac > 0 && ftmax_is_symbol(at))
	{
	  ftmax_symbol_t name = ftmax_get_symbol(at);

    fluidmax_load((t_object *)self, NULL, 1, at);
	}
	
  if(self->settings != NULL)
  {
  	fluid_settings_setnum(self->settings, "synth.midi-channels", midi_channels);
  	fluid_settings_setnum(self->settings, "synth.polyphony", polyphony);
  	fluid_settings_setnum(self->settings, "synth.gain", 0.600000);
  	fluid_settings_setnum(self->settings, "synth.sample-rate", sys_getsr());
  
  	self->synth = new_fluid_synth(self->settings);
  	
  	if(self->synth != NULL)
    {
      fluid_synth_set_reverb_on(self->synth, 0);
      fluid_synth_set_chorus_on(self->synth, 0);      
    
      if(ac > 0 && ftmax_is_symbol(at))
        fluidmax_load((t_object *)self, NULL, ac, at);

      return self;
    }

		delete_fluid_settings(self->settings);
  }

  error("fluidsynth~: cannot create FluidSynth core");
  
  return NULL;
}

static void
fluidmax_free(t_pxobject *o)
{
  fluidmax_t *self = (fluidmax_t *)o;

	if(self->settings != NULL )
		delete_fluid_settings(self->settings);

	if(self->synth != NULL )
		delete_fluid_synth(self->synth);

	dsp_free(o);
}

void 
main(void)
{
  setup(&fluidmax_class, (method)fluidmax_new, (method)fluidmax_free, (short)sizeof(fluidmax_t), 0, A_GIMME, 0);
  dsp_initclass();

  addmess((method)fluidmax_dsp, "dsp", A_CANT, 0);

  addmess((method)fluidmax_version, "version", 0);
  addmess((method)fluidmax_print, "print", A_GIMME, 0);

	addmess((method)fluidmax_load, "load", A_GIMME, 0);
	addmess((method)fluidmax_unload, "unload", A_GIMME, 0);
	addmess((method)fluidmax_reload, "reload", A_GIMME, 0);
  addmess((method)fluidmax_info, "info", A_GIMME, 0);
	
	addmess((method)fluidmax_panic, "panic", A_GIMME, 0);
	addmess((method)fluidmax_reset, "reset", A_GIMME, 0);
	addmess((method)fluidmax_mute, "mute", A_GIMME, 0);
	addmess((method)fluidmax_unmute, "unmute", 0);

	addmess((method)fluidmax_reverb, "reverb", A_GIMME, 0);
	addmess((method)fluidmax_chorus, "chorus", A_GIMME, 0);	
	addmess((method)fluidmax_gain, "gain", A_GIMME, 0);	
		
	addmess((method)fluidmax_note, "note", A_GIMME, 0);
	addmess((method)fluidmax_list, "list", A_GIMME, 0);

	addmess((method)fluidmax_control_change, "control", A_GIMME, 0);
	addmess((method)fluidmax_mod, "mod", A_GIMME, 0);
	
	addmess((method)fluidmax_pitch_bend, "bend", A_GIMME, 0);
	addmess((method)fluidmax_pitch_bend_wheel, "wheel", A_GIMME, 0);

	addmess((method)fluidmax_program_change, "program", A_GIMME, 0);
	addmess((method)fluidmax_bank_select, "bank", A_GIMME, 0);
	addmess((method)fluidmax_select, "select", A_GIMME, 0);

  sym_on = ftmax_new_symbol("on");
  sym_off = ftmax_new_symbol("off");
  sym_gain = ftmax_new_symbol("gain");
  sym_channels = ftmax_new_symbol("channels");
  sym_channel = ftmax_new_symbol("channel");
  sym_soundfonts = ftmax_new_symbol("soundfonts");
  sym_soundfont = ftmax_new_symbol("soundfont");
  sym_presets = ftmax_new_symbol("presets");
  sym_preset = ftmax_new_symbol("preset");
  sym_reverb = ftmax_new_symbol("reverb");
  sym_chorus = ftmax_new_symbol("chorus");
	
  fluidmax_version(NULL);
}
