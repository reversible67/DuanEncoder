#pragma once
#include <QThread>
#include <QString>
#include <QObject>
#include <SDL2/SDL.h>

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/error.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
}

class PlayerThread : public QThread
{
	Q_OBJECT
public:
	explicit PlayerThread(QObject* parent = nullptr);
	~PlayerThread() override;

	void setFilePath(const QString& filepath);
	void stopPlayback();

protected:
	void run() override;

signals:
	void playLog(const QString& log);
	void playError(const QString& error);
	void playFinished();
	void frameReady(SDL_Texture* texture, int errnum);

private:
	void printError(const char* msg, int errnum);
	QString m_filePath;
	bool m_stopFlag;
	SDL_Window* m_sdlWindow;
	SDL_Renderer* m_sdlRenderer;
	SDL_Texture* m_sdlTexture;
};