﻿#include <QDebug>
#include <QBuffer>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextFrame>
#include <QTextImageFormat>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QPainter>

#include "qrichedit.hpp"
#include "giftextobject.hpp"

#include <avproto/message.hpp>


void QGIFObjectInterface::drawObject(QPainter* painter, const QRectF& rect, QTextDocument* doc, int posInDocument, const QTextFormat& format)
{
	QUrl gifname = format.property(Property_Image_Name()).toString();

	auto qmovie = m_parent->m_gif[gifname];

	painter->drawImage(rect, qmovie->currentImage());
}

QSizeF QGIFObjectInterface::intrinsicSize(QTextDocument* doc, int posInDocument, const QTextFormat& format)
{
	QUrl gifname = format.property(Property_Image_Name()).toString();

	auto qmovie = m_parent->m_gif[gifname];

	return qmovie->currentImage().size();
}

QRichEdit::QRichEdit(QWidget*parent)
	: QTextEdit(parent)
{
	m_dropped_image_tmp_idx = 0;
	m_hasHeightForWidth = QTextEdit::hasHeightForWidth();

	m_obj_interface_gif = new QGIFObjectInterface(this);
	document()->documentLayout()->registerHandler(GIFTextFormat, dynamic_cast<QObject*>(m_obj_interface_gif));
}

QRichEdit::~QRichEdit()
{
	delete m_obj_interface_gif;
}

bool QRichEdit::canInsertFromMimeData(const QMimeData *source) const
{
	if (source->hasUrls())
	{
		// 只接受图片文件
		for (QUrl url : source->urls())
		{
			if (url.isLocalFile())
			{
				// 查询文件的 Mime 类型
				QMimeType type = m_minedb.mimeTypeForUrl(url);
				auto name = type.name();
				if (name == "image/png" || name == "image/jpeg" || name == "image/bmp" || name == "image/gif")
				{
					return true;
				}
			}
		}
	}
	return QTextEdit::canInsertFromMimeData(source);
}

void QRichEdit::insertFromMimeData(const QMimeData *source)
{
	if (source->hasImage())
	{
		QUrl url(QString("dropped_image_%1").arg(m_dropped_image_tmp_idx++));
		QByteArray ba;
		QImage img;
		{ QBuffer io(&ba);
		io.open(QIODevice::WriteOnly);
		img = qvariant_cast<QImage>(source->imageData());
		img.save(&io, "PNG");
		}
		auto imgdata = std::make_shared<image_data>(ba);
		auto inserted = m_image_raw_data.insert(std::make_pair(url, imgdata));
		dropImage(url, img);
	}
	else if (source->hasUrls())
	{
		for (QUrl url : source->urls())
		{
			if (url.isLocalFile())
			{
				QUrl tmpurl(QString("image_%1").arg(m_dropped_image_tmp_idx++));
				QFile imgfile(url.toLocalFile());

				if (!imgfile.open(QIODevice::ReadOnly))
					return;

				auto imgdata = std::make_shared<image_data>(imgfile.readAll());

				// 查询文件的 Mime 类型
				QMimeType type = m_minedb.mimeTypeForUrl(url);
				auto name = type.name();
				if (name == "image/png" || name == "image/jpeg" || name == "image/bmp")
				{
					auto inserted = m_image_raw_data.insert(std::make_pair(tmpurl, imgdata));
					QImage image;
					image.loadFromData(imgdata->get_bytes());
					dropImage(tmpurl, image);
				}
				else if (name == "image/gif")
				{
					auto inserted = m_image_raw_data.insert(std::make_pair(tmpurl, imgdata));

					dropGIF(tmpurl, new QMovie(inserted.first->second->get_io_device()));
				}
			}
		}
	}
	else
	{
		QTextEdit::insertFromMimeData(source);
	}
}

void QRichEdit::dropImage(const QUrl& url, const QImage& image)
{
	if (!image.isNull())
	{
		document()->addResource(QTextDocument::ImageResource, url, image);
		auto textcursor = textCursor();
		textcursor.insertImage(url.toString());
		setTextCursor(textcursor);

		Q_EMIT textChanged();
	}
}

void QRichEdit::dropGIF(const QUrl& url, QMovie* gif)
{
	m_gif[url].reset(gif);
	m_is_gif.append(url);

	auto cursor = textCursor();

	QTextCharFormat charfmt = cursor.charFormat();

	charfmt.setObjectType(GIFTextFormat);
	charfmt.setProperty(m_obj_interface_gif->Property_Image_Name(), url.toString());

	cursor.insertText(QString(QChar::ObjectReplacementCharacter), charfmt);
	setTextCursor(cursor);
	QObject::connect(gif, SIGNAL(updated(const QRect &)), viewport(), SLOT(update()), Qt::QueuedConnection);
	gif->start();
}

const QByteArray& QRichEdit::get_image_data(const QString& name)
{
	return m_image_raw_data[name]->get_bytes();
}

void QRichEdit::clear()
{
	for (auto &i : m_gif)
	{
		i.second->stop();
	}
	m_gif.clear();

	QTextEdit::clear();
	m_is_gif.clear();
	m_image_raw_data.clear();
}

message::message_packet QRichEdit::get_content()
{
	message::message_packet impkt;

	QTextDocument* doc = document();
	int blockcount = doc->blockCount();

	for (QTextBlock blk = doc->begin(); blk != doc->end(); blk = blk.next())
	{
		for (auto blkit = blk.begin(); blkit != blk.end(); ++blkit)
		{
			QTextFragment docfrag = blkit.fragment();
			auto txt = docfrag.text();
			if (docfrag.charFormat().isImageFormat())
			{
				QTextImageFormat imgformat = docfrag.charFormat().toImageFormat();

				const QByteArray& ba = get_image_data(imgformat.name());

				// nice, 弄到 impkt 里
				message::img_message item_content;

				item_content.set_animated(m_is_gif.contains(imgformat.name()));

				item_content.set_image(ba.data(), ba.length());

				impkt.mutable_avim()->Add()->mutable_item_image()->CopyFrom(item_content);
			}
			else if(docfrag.charFormat().isCharFormat() && docfrag.charFormat().objectType() == GIFTextFormat)
			{
				QTextCharFormat charformat = docfrag.charFormat().toCharFormat();

				const QByteArray& ba = get_image_data(charformat.property(m_obj_interface_gif->Property_Image_Name()).toString());

				// nice, 弄到 impkt 里
				message::img_message item_content;

				item_content.set_animated(1);

				item_content.set_image(ba.data(), ba.length());

				impkt.mutable_avim()->Add()->mutable_item_image()->CopyFrom(item_content);
			}
			else
			{
				message::text_message item_content;
				item_content.set_text(txt.toStdString());

				impkt.mutable_avim()->Add()->mutable_item_text()->CopyFrom(item_content);
			}
		}
	}
	return impkt;
}

void QRichEdit::set_content(message::message_packet msg)
{
	for (message::avim_message im_message_item : msg.avim())
	{
		if (im_message_item.has_item_text())
		{
			message::text_message text_message = im_message_item.item_text();
			std::string text = text_message.text();
			textCursor().insertText(QString::fromStdString(text));
		}
		else if (im_message_item.has_item_image())
		{
			message::img_message item_image = im_message_item.item_image();

			QUrl tmpurl(QString("image_%1").arg(m_dropped_image_tmp_idx++));
			auto img_data = std::make_shared<image_data>(
				QByteArray::fromRawData(item_image.image().data(),
					item_image.image().length()
				)
			);

			bool use_gif = false;

			if (im_message_item.item_image().has_animated())
			{
				use_gif = im_message_item.item_image().animated();
			}
			else
			{
				// 检查是 GIF 还是普通图片
				auto type = m_minedb.mimeTypeForData(img_data->get_bytes());
				use_gif = (type.name() == "image/gif");
			}

			img_data = std::make_shared<image_data>(
				QByteArray::fromRawData(item_image.image().data(),
					item_image.image().length()
				)
			);
			auto inserted = m_image_raw_data.insert(std::make_pair(tmpurl, img_data));
			if (use_gif)
			{
				dropGIF(tmpurl, new QMovie(img_data->get_io_device()));
			}else
			{
				QImage img;
				img.loadFromData(img_data->get_bytes());
				dropImage(tmpurl, img);
			}
		}
	}

	Q_EMIT textChanged();
}

bool QRichEdit::hasHeightForWidth() const
{
	bool is = QTextEdit::hasHeightForWidth();
	return m_hasHeightForWidth;
}

void QRichEdit::set_hasHeightForWidth(bool v)
{
	m_hasHeightForWidth = v;
}

int QRichEdit::heightForWidth(int w) const
{
	int h;
	if (m_hasHeightForWidth)
	{
		document()->size().setWidth(w);
		document()->setTextWidth(w-5);
		h = document()->size().height() + 5;
	}
	else
	{
		h = QWidget::heightForWidth(w);
	}
	return h;
}

QSize QRichEdit::sizeHint() const
{
	if (m_hasHeightForWidth)
	{
		return document()->size().toSize();
	}
	return QAbstractScrollArea::sizeHint();
}
