/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test UI
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <freerdp/log.h>

#define TAG CLIENT_TAG("sample")

struct tf_context
{
	rdpContext _p;
};
typedef struct tf_context tfContext;

static int tf_context_new(freerdp* instance, rdpContext* context)
{
	context->channels = freerdp_channels_new();
	return 0;
}

static void tf_context_free(freerdp* instance, rdpContext* context)
{
	if (context && context->channels)
	{
		freerdp_channels_close(context->channels, instance);
		freerdp_channels_free(context->channels);
	}
}

static BOOL tf_begin_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;
	gdi->primary->hdc->hwnd->invalid->null = 1;
    return TRUE;
}

static BOOL tf_end_paint(rdpContext* context)
{
	rdpGdi* gdi = context->gdi;

	if (gdi->primary->hdc->hwnd->invalid->null)
		return TRUE;
	return TRUE;
}

static BOOL tf_pre_connect(freerdp* instance)
{
	rdpSettings* settings;


	settings = instance->settings;

	settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
	settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
	settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
	settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
	settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
	settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;

	freerdp_channels_pre_connect(instance->context->channels, instance);

	return TRUE;
}

static BOOL tf_post_connect(freerdp* instance)
{
	gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);

	instance->update->BeginPaint = tf_begin_paint;
	instance->update->EndPaint = tf_end_paint;

	freerdp_channels_post_connect(instance->context->channels, instance);

	return TRUE;
}

static void* tf_client_thread_proc(freerdp* instance)
{
	DWORD nCount;
	DWORD status;
	HANDLE handles[64];

	if (!freerdp_connect(instance))
	{
		WLog_ERR(TAG, "connection failure");
		return NULL;
	}

	while (!freerdp_shall_disconnect(instance))
	{
		nCount = freerdp_get_event_handles(instance->context, &handles[0], 64);

		if (nCount == 0)
		{
				WLog_ERR(TAG, "%s: freerdp_get_event_handles failed", __FUNCTION__);
				break;
		}

		status = WaitForMultipleObjects(nCount, handles, FALSE, 100);

		if (status == WAIT_FAILED)
		{
				WLog_ERR(TAG, "%s: WaitForMultipleObjects failed with %lu", __FUNCTION__, status);
				break;
		}

		if (!freerdp_check_event_handles(instance->context))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP event handles");
			break;
		}
	}

	freerdp_disconnect(instance);

	ExitThread(0);
	return NULL;
}

int main(int argc, char* argv[])
{
	int status;
	HANDLE thread;
	freerdp* instance;

	instance = freerdp_new();
	if (!instance)
	{
		WLog_ERR(TAG, "Couldn't create instance");
		exit(1);
	}
	instance->PreConnect = tf_pre_connect;
	instance->PostConnect = tf_post_connect;

	instance->ContextSize = sizeof(tfContext);
	instance->ContextNew = tf_context_new;
	instance->ContextFree = tf_context_free;
	if (freerdp_context_new(instance) != 0)
	{
		WLog_ERR(TAG, "Couldn't create context");
		exit(1);
	}

	status = freerdp_client_settings_parse_command_line(instance->settings, argc, argv, FALSE);

	if (status < 0)
	{
		exit(0);
	}

	freerdp_client_load_addins(instance->context->channels, instance->settings);

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			tf_client_thread_proc, instance, 0, NULL);

	WaitForSingleObject(thread, INFINITE);
	freerdp_context_free(instance);
	freerdp_free(instance);

	return 0;
}
