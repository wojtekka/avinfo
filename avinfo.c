/*
 *  (C) Copyright 2004 Wojtek Kaniewski <wojtekka@toxygen.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avi.h"
#include "audio.h"
#include "video.h"

#ifdef WIN32
#include <windows.h>
#include <commdlg.h>
#endif

const char *fourcc(FOURCC tag)
{
	static unsigned char buf[5];
	int i;

	buf[0] = tag & 255;
	buf[1] = (tag >> 8) & 255;
	buf[2] = (tag >> 16) & 255;
	buf[3] = (tag >> 24) & 255;
	buf[4] = 0;

	for (i = 0; i < 4; i++)
		if (buf[i] < 32 || buf[i] > 127)
			buf[i] = '?';

	return buf;
}

#ifdef WIN32
const char *opendialog()
{
	static char buf[4096];
	OPENFILENAMEA o;

	memset(&o, 0, sizeof(o));
	memset(&buf, 0, sizeof(buf));

	o.lStructSize = sizeof(o);
	o.lpstrFile = buf;
	o.lpstrFilter = "AVI files (*.avi)\0*.avi\0All files (*.*)\0*.*\0";
	o.nMaxFile = sizeof(buf) - 1;
	o.Flags = OFN_HIDEREADONLY;

	if (GetOpenFileName(&o))
		return buf;
	else
		return NULL;
}
#endif

void error(const char *s)
{
#ifdef WIN32
	MessageBox(0, s, "AVInfo", MB_ICONERROR);
#else
	fprintf(stderr, "%s\n", s);
#endif
	exit(1);
}

DWORD get32(FILE *f)
{
	DWORD result;
	
	if (fread(&result, sizeof(DWORD), 1, f) != 1)
		error("Premature end of file");

	return result;
}

WORD get16(FILE *f)
{
	WORD result;
	
	if (fread(&result, sizeof(WORD), 1, f) != 1)
		error("Premature end of file");

	return result;
}

#ifdef WIN32
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iShowCmd)
#else
int main(int argc, char **argv)
#endif
{
	FILE *f;
	FOURCC tag, list = 0, vids = 0, type = 0;
	DWORD size, hz = 0;
	WORD auds = 0, ch = 0;
	double fps = 0;
	int width = 0, height = 0, i;
	char buf[1024], fourcc_vids[5];
	const char *_vids = "Unknown codec", *_auds = "Unknown codec";
	const char *filename = NULL;

#ifndef WIN32
	if (argc == 2)
		filename = argv[1];
	else
		error("Not enough parameters");
#else
	if (strcmp(szCmdLine, ""))
		filename = szCmdLine;

	if (!filename)
		filename = opendialog();

	if (!filename)
		exit(0);
#endif

	f = fopen(filename, "rb");

	if (!f)
		error("Unable to open file");

	tag = get32(f);
	size = get32(f);

	if (tag != MKTAG('R','I','F','F'))
		error("Not a RIFF file");

	tag = get32(f);

	if (tag != MKTAG('A','V','I',' '))
		error("Not an AVI file");

	while (!feof(f)) {
		tag = get32(f);
		size = get32(f);

		if (!tag)
			error("Invalid file format");

		if (tag == MKTAG('L','I','S','T')) {
			if ((list = get32(f)) == MKTAG('m','o','v','i')) {
				fclose(f);
				break;
			}
			continue;
		}
		
		if (tag == MKTAG('a','v','i','h')) {
			AVIMAINHEADER avih;

			fread(&avih, sizeof(avih), 1, f);

			width = avih.dwWidth;
			height = avih.dwHeight;

			fseek(f, -sizeof(avih), SEEK_CUR);
		}

		if (tag == MKTAG('s','t','r','h')) {
			AVISTREAMHEADER strh;

			fread(&strh, sizeof(strh), 1, f);
			
			if ((type = strh.fccType) == MKTAG('v','i','d','s')) {
				vids = strh.fccHandler;
				fps = (double) strh.dwRate / (double) strh.dwScale;
			}

			fseek(f, -sizeof(strh), SEEK_CUR);
		}

		if (tag == MKTAG('s','t','r','f')) {
			if (type == MKTAG('a','u','d','s')) {
				WAVEFORMATEX wave;

				fread(&wave, sizeof(wave), 1, f);

				hz = wave.nSamplesPerSec;
				ch = wave.nChannels;
				auds = wave.wFormatTag;
				
				fseek(f, -sizeof(wave), SEEK_CUR);
			}
			
			if (type == MKTAG('v','i','d','s') && !vids) {
				BITMAPINFOHEADER bm;

				fread(&bm, sizeof(bm), 1, f);

				vids = bm.biCompression;
				
				fseek(f, -sizeof(bm), SEEK_CUR);
			}
		}

		fseek(f, size + (size & 1), SEEK_CUR);
	}

	for (i = 0; audio_formats[i].tag; i++)
		if (audio_formats[i].tag == auds)
			_auds = audio_formats[i].descr;

	strcpy(fourcc_vids, fourcc(vids));

	for (i = 0; video_formats[i].tag; i++)
		if (!strcasecmp(video_formats[i].tag, fourcc_vids))
			_vids = video_formats[i].descr;

	if (hz || ch || auds)
		sprintf(buf,
			"Video: %dx%d, %.2f fps, %s [%s]\n"
			"Audio: %ldHz, %dch, %s [0x%.4x]",
			width, height, fps, _vids, fourcc(vids),
			hz, ch, _auds, auds);
	else
		sprintf(buf,
			"Video: %dx%d, %.2f fps, %s [%s]\n"
			"No audio",
			width, height, fps, _vids, fourcc(vids));

#ifdef WIN32
	MessageBox(0, buf, filename, MB_ICONINFORMATION);
#else
	printf("%s\n", buf);
#endif

	return 0;
}
