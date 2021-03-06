
#define STATUS_HEIGHT			13

#include <Application.h>
#include <Font.h>

#include <stdio.h>

#include "ClientWindow.h"
#include "StatusView.h"

StatusView::StatusView (BRect frame)
	
	: BView (
		BRect (
			frame.left,
			frame.bottom - STATUS_HEIGHT,
			frame.right,
			frame.bottom),
		"status",
		B_FOLLOW_LEFT_RIGHT | B_FOLLOW_BOTTOM,
		B_WILL_DRAW)
{
	BFont font (be_plain_font);
	font_height fh;

	font.GetHeight (&fh);
	while (fh.ascent + fh.leading > STATUS_HEIGHT - 2)
	{
		font.SetSize (font.Size() - 2);
		font.GetHeight (&fh);
	}

	SetFont (&font);

	SetViewColor (216, 216, 216,.255);
	SetLowColor (216, 216, 216, 255);
	SetHighColor (0, 0, 0, 255);
}

StatusView::~StatusView (void)
{
	while (!items.IsEmpty())
	{
		#ifdef __x86_64__
		StatusItem *item ((StatusItem *)items.RemoveItem (0));
		#else
		StatusItem *item ((StatusItem *)items.RemoveItem (0L));
		#endif

		delete item;
	}
}

void
StatusView::AddItem (StatusItem *item, bool erase)
{
	StatusItem *last ((StatusItem *)items.LastItem());
	float width (3.0);

	if (last)
		width = last->frame.right + 8.0;

	if (item->label.Length())
		width += StringWidth (item->label.String()) + 3;

	item->frame.top    = 2.0;
	item->frame.bottom = Bounds().Height();
	item->frame.left   = width;
	item->frame.right  = width + StringWidth (item->value.String());

	if (erase) item->value = "";

	items.AddItem (item);
}

StatusItem *
StatusView::ItemAt (int32 which) const
{
	return (StatusItem *)items.ItemAt (which);
}

void
StatusView::SetItemValue (int32 which, const char *value)
{
	StatusItem *item (ItemAt (which));
	StatusItem *nextitem;
	if (item)
	{
		item->value = value;
		item->frame.right = item->frame.left + StringWidth(value);
		for (int32 i = which+1; (nextitem = ItemAt(i)) != NULL ; i++)
		{
			nextitem->frame.left = item->frame.right + 8.0 + StringWidth(nextitem->label.String());
			nextitem->frame.right = nextitem->frame.left + StringWidth(nextitem->value.String());
			item = nextitem;
		}
		Invalidate ();
	}
}

void
StatusView::Draw (BRect update)
{
	SetDrawingMode (B_OP_COPY);
	SetHighColor (131, 131, 131, 255);
	StrokeLine (
		BPoint (update.left, Bounds().top),
		BPoint (update.right, Bounds().top));
	SetHighColor (255, 255, 255, 255);
	StrokeLine (
		BPoint (update.left, Bounds().top + 1),
		BPoint (update.right, Bounds().top + 1));

	float width (5.0);
	font_height fh;

	GetFontHeight (&fh);

	SetDrawingMode (B_OP_OVER);
	SetHighColor (0, 0, 0, 255);
	
	for (int32 i = 0; i < items.CountItems(); ++i)
	{
		if (i)
		{
			DrawSplit (width += 3);
			width += 5;
		}
		
		StatusItem *item (ItemAt (i));

		if (item->label.Length())
		{
			DrawString (item->label.String(),
				BPoint (width, fh.ascent + fh.leading + 2));
		}

		if (item->alignment == STATUS_ALIGN_RIGHT)
			width = item->frame.right - StringWidth (item->value.String());
		else
			width = item->frame.left;
				
		DrawString (item->value.String(),
			BPoint (width, fh.ascent + fh.leading + 2));
		width = item->frame.right;
	}
}


void
StatusView::DrawSplit (float x)
{
	BRect bounds (Bounds());

	PushState();

	SetDrawingMode (B_OP_COPY);
	SetHighColor (131, 131, 131, 255);
	StrokeLine (
		BPoint (x, bounds.top + 2.0),
		BPoint (x, bounds.bottom));

	SetHighColor (255, 255, 255, 255);
	StrokeLine (
		BPoint (x + 1, bounds.top + 2.0),
		BPoint (x + 1, bounds.bottom));
	
	PopState();
}

StatusItem::StatusItem (
	const char *label_,
	const char *value_,
	int32 alignment_)
{
	label = label_ ? label_ : "";
	value = value_ ? value_ : "";
	alignment = alignment_;
}

StatusItem::~StatusItem (void)
{
}
