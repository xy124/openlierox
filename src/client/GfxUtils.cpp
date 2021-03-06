/*
 *  GfxUtils.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 03.12.09.
 *  code under LGPL
 *
 */


#include "GfxPrimitives.h"
#include "Geometry.h"
#include "Timer.h"
#include "Mutex.h"
#include "LieroX.h"
#include "AuxLib.h"
#include "Condition.h"

void DrawLoadingAni(SDL_Surface* bmpDest, int x, int y, int rx, int ry, Color fg, Color bg, LoadingAniType type) {
	switch(type) {
		case LAT_CIRCLES: {
			static const int STEPS = 12;
			int cur = int(((GetTime().milliseconds() % 1000)) * STEPS * 0.001f);
			for(int i = 0; i < STEPS; ++i) {
				const float a = (float)PI * 2.0f * i / STEPS;
				VectorD2<float> p( cosf(a) * rx, sinf(a) * ry );
				
				DrawCircleFilled(bmpDest, int(x + p.x), int(y + p.y), rx / 5, ry / 5, (i == cur) ? fg : bg);
			}
			break;
		}
		case LAT_CAKE: {
			static const int STEPS = 6;
			int cur = int(((GetTime().milliseconds() % 500)) * STEPS * 0.002f);
			for(int i = 0; i < STEPS; ++i) {
				const float angle = (float)PI * 2.0f * i / STEPS;
				const float a = angle + (float)PI * 2.0f * -0.5f / STEPS;
				const float b = angle + (float)PI * 2.0f * 0.5f / STEPS;
				Polygon2D poly;
				poly.startPointAdding();
				poly.addPoint(VectorD2<float>(cosf(a) * rx, sinf(a) * ry));
				poly.addPoint(VectorD2<float>(cosf(b) * rx, sinf(b) * ry));
				poly.addPoint(VectorD2<int>(0,0));
				poly.endPointAdding();
				float colf = float((STEPS + cur - i) % STEPS) / float(STEPS - 1);
				Color col = bg * colf + fg * (1.0f - colf);
				poly.drawFilled(bmpDest, x, y, col);
			}
			break;
		}
	}
}


struct ScopedBackgroundLoadingAni::Data {
	ThreadPoolItem* thread;
	Mutex mutex;
	bool quit;
	Condition breakSig;
	SmartPointer<SDL_Surface> screenBackup;
	
	Data() : thread(NULL), quit(false) {
		screenBackup = gfxCreateSurface(640,480);
		CopySurface(screenBackup.get(), VideoPostProcessor::videoSurface(), 0,0,0,0,640,480);
	}
	~Data() {
		CopySurface(VideoPostProcessor::videoSurface(), screenBackup.get(), 0,0,0,0,640,480);	
	}
};

ScopedBackgroundLoadingAni::ScopedBackgroundLoadingAni(int x, int y, int rx, int ry, Color fg, Color bg, LoadingAniType type) {
	data = NULL;
	if(bDedicated) return;
	
	data = new Data();
	struct Animator : Action {
		int x, y, rx, ry;
		LoadingAniType type;
		Color fg, bg;
		Data* data;
		
		int handle() {
			Mutex::ScopedLock lock(data->mutex);
			while(!data->quit) {
				// Kind of a hack: we update the time so that mainlockdetector does not break this
				// loading, no matter how slow it is.
				// As we are only loading the mod/map, we can savly access tLX->currentTime.
				tLX->currentTime = GetTime();
				
				DrawImageEx(VideoPostProcessor::videoSurface(), data->screenBackup, 0,0,640,480);
				DrawLoadingAni(VideoPostProcessor::videoSurface(), x, y, rx, ry, fg, bg, type);
				doVideoFrameInMainThread();
				
				data->breakSig.wait(data->mutex, 10);
			}
			return 0;
		}
	};
	
	Animator* anim = new Animator();
	anim->x = x;
	anim->y = y;
	anim->rx = rx;
	anim->ry = ry;
	anim->fg = fg;
	anim->bg = bg;
	anim->type = type;
	anim->data = data;
	
	data->thread = threadPool->start(anim, "Background Loading animation");
}

ScopedBackgroundLoadingAni::~ScopedBackgroundLoadingAni() {
	if(data) {
		{
			Mutex::ScopedLock lock(data->mutex);
			data->quit = true;
			data->breakSig.signal();
		}
		threadPool->wait(data->thread);
		delete data; data = NULL;
	}
}


