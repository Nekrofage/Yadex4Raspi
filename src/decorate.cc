/* decorate.cc
    very simple decorate parser to allow users to add items
    by creating a decorate lump

    6-17-2007
*/

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "yadex.h"
#include "things.h"
#include "game.h"
#include "acolours.h"

#include "wadfile.h"
#include "wadlist.h"
#include "wads.h"
#include "wads2.h"

#include "decorate.h"

char        readbuf[200];

int         ntoks;
char       *token[MAX_TOKENS];
int         counter;
int         in_token;
const char *iptr;
char       *optr;
int        current;
float        scale;

bool        getsprite = false;
bool        addeditem = false;

thingdef_t  buf;

void
parse_line(void) {
	// duplicate the buffer
	char* buffer = (char *) malloc (strlen (readbuf) + 1);
	if (!buffer)
		fatal_error ("not enough memory");

	// break the line into whitespace-separated tokens.
	for (in_token = 0, iptr = readbuf, optr = buffer, ntoks = 0;; iptr++) {
		if (*iptr == '\n' || *iptr == '\0') {
			if (in_token)
				*optr = '\0';
			break;
		}
		// First character of token
		else if (! in_token && ! isspace (*iptr)) {
			token[ntoks] = optr;
			ntoks++;
			in_token = 1;
			*optr++ = *iptr;
		}
		// First space between two tokens
		else if (in_token && (isspace (*iptr) || *iptr == '{' || *iptr == '}')) {
			*optr++ = '\0';
			in_token = 0;
		}
		// Character in the middle of a token
		else if (in_token)
			*optr++ = *iptr;
	}
	free (buffer);
}

// this function pushes a thing definition onto the stack of things
// this is done only if buf.number is set,which means we are talking
// about a complete new,placable thing
void
update_thingdefs(void) {
    if (buf.number) {
        if (al_lwrite (thingdef, &buf))
             fatal_error ("LGD4 (%s)", al_astrerror (al_aerrno));
        addeditem = true;
    }

    buf.number     = 0;
    buf.thinggroup = '*';
    buf.flags      = 0;
    buf.radius     = 16;
    buf.desc       = 0;
    buf.sprite     = 0;
    buf.scale      = 1;
}

void
read_decorate (void) {
	int fd;
	FILE* tempfile;
	const char* templ = "/tmp/decorate.XXXXXXXXX";
	char *filename;

	filename = (char*) calloc(sizeof(char), strlen(templ) + 1);
	strlcpy(filename, templ, strlen(templ));

	if ((fd = mkstemp(filename)) < 0) {
		fprintf(stderr, "can't create temp file from template \"%s\": %s\n", templ, strerror(errno));
		free(filename);
		return;
	}

	if (unlink(filename) != 0) {
		fprintf(stderr, "can't unlink temporary file \"%s\": %s\n", filename, strerror(errno));
		free(filename);
		return;
	}

	free(filename);
	if ((tempfile = fdopen(fd, "rw")) == NULL) {
		fprintf(stderr, "can't open temp file: %s\n", strerror(errno));
		return;
	}

	printf("Reading decorate lump.\n");

	// FIXME:
	// the decorate lump should directly be read from the
	// wad and not from a temporary file
	SaveEntryToRawFile(tempfile, "DECORATE");
	fseek(tempfile, 0L, SEEK_SET);

	while (fgets(readbuf, sizeof readbuf, tempfile)) {
		current = 0;
		parse_line();

		/* process line */
		if (ntoks == 0) {
			continue;
		}

		// Actor definition starts
		if (! y_strnicmp (token[0], "actor",5)) {
			update_thingdefs();

			counter = 0;
			while (counter < ntoks) {
				buf.number = atoi (token[counter]);
				if (!buf.number)
					counter++;
				else
					counter = ntoks+1;
			}

			buf.thinggroup = '*';
			buf.flags      = 0;
			buf.desc       = token[1];
			// the radius
		} else if (! y_strnicmp(token[0], "radius",6)) {
			buf.radius    = atoi(token[1]) * 2;
			// here the state from which the sprite is read is recognized
		} else if (! y_strnicmp(token[0], STATE,STATELEN + 1)) {
			getsprite    = true;
			current        = 1;
			// the scale of the thing's sprite
		} else if (! y_strnicmp(token[0], "scale",5)) {
			buf.scale        = atof(token[1]);
		} else if ((! y_strnicmp(token[0], "renderstyle",11))
				&& ((! y_strnicmp(token[1], "optfuzzy",8))
					|| (! y_strnicmp(token[1], "fuzzy",5)))) {
			buf.flags     = 's';
		}
		// if a state was recognized,get the next token with a length
		// of SPRITELEN and set it as the thing's sprite
		if (getsprite == true) {
			if (strlen(token[current]) == SPRITELEN) {
				buf.sprite = token[current];
				getsprite = false;
			}
		}
	}

	// if things were found inside the decorate file, add them to a group
	// called "Decorate Items"
	thinggroup_t group;
	if (addeditem) {
		group.thinggroup = '*';
		group.acn = add_app_colour (rgb_c ('2','3','4'));
		group.desc = "Decorate Items";

		if (al_lwrite (thinggroup, &group))
			fatal_error ("LGD5 (%s)", al_astrerror (al_aerrno));

		update_thingdefs();

		delete_things_table();
		create_things_table();
	}

	fclose(tempfile);
	remove(filename);

	return;
}
