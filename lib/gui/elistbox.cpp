#include <lib/gui/elistbox.h>
#include <lib/gui/elistboxcontent.h>
#include <lib/gui/eslider.h>
#include <lib/actions/action.h>
#include <lib/base/nconfig.h>
#include <lib/base/eerror.h> // Add for debug output
#ifdef USE_LIBVUGLES2
#include "vuplus_gles.h"
#endif

eRect eListbox::defaultPadding = eRect(1, 1, 1, 1);
int eListbox::defaultScrollBarWidth = eListbox::DefaultScrollBarWidth;
int eListbox::defaultScrollBarOffset = eListbox::DefaultScrollBarOffset;
int eListbox::defaultScrollBarBorderWidth = eListbox::DefaultScrollBarBorderWidth;
int eListbox::defaultScrollbarRadius = 0;
int eListbox::defaultPageSize = eListbox::DefaultPageSize;
int eListbox::defaultItemRadius[4] = {0, 0, 0, 0};
uint8_t eListbox::defaultItemRadiusEdges[4] = {0, 0, 0, 0};
uint8_t eListbox::defaultScrollBarScroll = eListbox::DefaultScrollBarScroll;
uint8_t eListbox::defaultScrollBarMode = eListbox::DefaultScrollBarMode;
uint8_t eListbox::defaultScrollbarRadiusEdges = 0;
bool eListbox::defaultWrapAround = eListbox::DefaultWrapAround;

eListbox::eListbox(eWidget *parent) : eWidget(parent), m_prev_scrollbar_page(-1), m_scrollbar_mode(showNever), m_scrollbar_scroll(byPage),
									  m_content_changed(false), m_enabled_wrap_around(false), m_itemwidth_set(false), m_itemheight_set(false), m_scrollbar_width(10),
									  m_scrollbar_height(10), m_scrollbar_length(0), m_top(0), m_left(0), m_selected(0), m_itemheight(25), m_itemwidth(25),
									  m_orientation(orVertical), m_max_columns(0), m_max_rows(0), m_selection_enabled(1), m_page_size(0), m_item_alignment(0), xOffset(0), yOffset(0),
									  m_native_keys_bound(false), m_first_selectable_item(-1), m_last_selectable_item(-1), m_scrollbar(nullptr)
{
	m_scrollbar_width = eListbox::defaultScrollBarWidth;
	m_scrollbar_height = eListbox::defaultScrollBarWidth; // TODO
	m_scrollbar_offset = eListbox::defaultScrollBarOffset;
	m_scrollbar_border_width = eListbox::defaultScrollBarBorderWidth;
	m_scrollbar_scroll = eListbox::defaultScrollBarScroll;
	m_enabled_wrap_around = eListbox::defaultWrapAround;
	m_scrollbar_mode = eListbox::defaultScrollBarMode;
	m_page_size = eListbox::defaultPageSize;

	memset(static_cast<void *>(&m_style), 0, sizeof(m_style));
	m_style.m_text_padding = eListbox::defaultPadding;
	m_style.m_selection_zoom = 1.0;
	m_style.m_selection_width = m_itemwidth;
	m_style.m_selection_height = m_itemheight;
	m_style.m_scrollbar_radius = eListbox::defaultScrollbarRadius;
	m_style.m_scrollbar_edges = eListbox::defaultScrollbarRadiusEdges;
	m_style.m_separator_size = eRect(1, -1, -1, 1);

	for (uint8_t x = 0; x < 4; x++)
	{
		m_style.m_gradient_set[x] = false;
		if (eListbox::defaultItemRadius[x] && eListbox::defaultItemRadiusEdges[x])
			setItemCornerRadiusInternal(x, eListbox::defaultItemRadius[x], eListbox::defaultItemRadiusEdges[x]);
		else
			setItemCornerRadiusInternal(x, 0, 0);
	}

	allowNativeKeys(true);

	if (m_scrollbar_mode != showNever)
		setScrollbarMode(m_scrollbar_mode);

	//m_scroll_timer = new eTimer(this);
	m_scroll_timer = eTimer::create(eApp);
	CONNECT(m_scroll_timer->timeout, eListbox::onScrollTimer);
	bool m_page_transition_active = false;
	int m_page_transition_offset = 0;
	int m_page_transition_direction = 0; // 1 = down (slide up), -1 = up (slide down)
}

eListbox::~eListbox()
{
	if (m_scrollbar)
		delete m_scrollbar;

	//if (m_scroll_timer)
		//delete m_scroll_timer;
	
	allowNativeKeys(false);
	
}

void eListbox::setScrollbarMode(uint8_t mode)
{
	m_scrollbar_mode = mode;
	if (m_scrollbar)
	{
		if (m_scrollbar_mode == showNever)
		{
			delete m_scrollbar;
			m_scrollbar = 0;
		}
	}
	else
	{
		m_scrollbar = new eSlider(this);
		m_scrollbar->setIsScrollbar();
		m_scrollbar->hide();
		m_scrollbar->setBorderWidth(m_scrollbar_border_width);
		m_scrollbar->setOrientation(m_orientation == orHorizontal ? eSlider::orHorizontal : eSlider::orVertical);
		m_scrollbar->setRange(0, 100);
		if (m_scrollbarbackgroundpixmap)
			m_scrollbar->setBackgroundPixmap(m_scrollbarbackgroundpixmap);
		if (m_scrollbarpixmap)
			m_scrollbar->setPixmap(m_scrollbarpixmap);
		if (m_style.is_set.scollbarborder_color)
			m_scrollbar->setBorderColor(m_style.m_scollbarborder_color);
		if (m_style.is_set.scrollbarforeground_color)
			m_scrollbar->setForegroundColor(m_style.m_scrollbarforeground_color);
		if (m_style.is_set.scrollbarbackground_color)
			m_scrollbar->setBackgroundColor(m_style.m_scrollbarbackground_color);
		if (m_style.m_scrollbar_radius)
			m_scrollbar->setCornerRadius(m_style.m_scrollbar_radius, m_style.m_scrollbar_edges);
		if (m_style.is_set.scrollbarforegroundgradient)
			m_scrollbar->setForegroundGradient(m_style.m_scrollbarforegroundgradient_colors, (m_orientation == orHorizontal) ? 2 : 1, false, true);
		if (m_style.is_set.scrollbarbackgroundgradient)
			m_scrollbar->setBackgroundGradient(m_style.m_scrollbarbackgroundgradient_colors, (m_orientation == orHorizontal) ? 2 : 1, false);
	}
}

void eListbox::setScrollbarScroll(uint8_t scroll)
{
	if (m_scrollbar && m_scrollbar_scroll != scroll)
	{
		m_scrollbar_scroll = scroll;
		updateScrollBar();
		return;
	}
	m_scrollbar_scroll = scroll;
}

void eListbox::setContent(iListboxContent *content)
{
	m_content = content;
	if (content)
		m_content->setListbox(this);
	entryReset();
}

void eListbox::allowNativeKeys(bool allow)
{
	if (m_native_keys_bound != allow)
	{
		ePtr<eActionMap> ptr;
		eActionMap::getInstance(ptr);
		if (allow)
			ptr->bindAction("ListboxActions", (int64_t)0, 0, this);
		else
			ptr->unbindAction(this, 0);
		m_native_keys_bound = allow;
	}
}

bool eListbox::atBegin()
{
	if (m_content && !m_selected)
		return true;
	return false;
}

bool eListbox::atEnd()
{
	if (m_content && m_content->size() == m_selected + 1)
		return true;
	return false;
}

// Deprecated
void eListbox::moveToEnd()
{
	eWarning("[eListbox] moveToEnd is deprecated. Use moveSelection or goBottom instead.");
	if (!m_content)
		return;
	/* move to last existing one ("end" is already invalid) */
	m_content->cursorEnd();
	m_content->cursorMove(-1);
	/* current selection invisible? */

	int topLeft = m_top;
	int maxItems = m_max_rows;

	if (m_orientation == orHorizontal)
	{
		topLeft = m_left;
		maxItems = m_max_columns;
	}

	if (topLeft + maxItems <= m_content->cursorGet())
	{
		int rest = m_content->size() % maxItems;
		if (rest)
			topLeft = m_content->cursorGet() - rest + 1;
		else
			topLeft = m_content->cursorGet() - maxItems + 1;
		if (topLeft < 0)
			topLeft = 0;
	}

	if (m_orientation == orHorizontal)
		m_left = topLeft;
	else
		m_top = topLeft;
}

void eListbox::moveSelectionTo(int index)
{
	if (m_content)
	{
		m_content->cursorSet(index);
		m_content_changed = true;
		moveSelection(justCheck + 100);
	}
}

void eListbox::setTopIndex(int index)
{
	if (m_content)
	{
		if (m_content->size() > index)
		{
			m_top = index;
			m_content_changed = true;
			moveSelection(justCheck);
		}
	}
}

int eListbox::getCurrentIndex()
{
	if (m_content && m_content->cursorValid())
		return m_content->cursorGet();
	return 0;
}

int eListbox::setScrollbarPosition()
{

	int width = size().width();
	int height = size().height();
	int x = xOffset;
	int y = yOffset;

	if (m_scrollbar_length == -1)
	{
		if (m_orientation == orHorizontal)
		{

			if (m_style.m_selection_width != m_itemwidth)
			{
				x += (m_style.m_selection_width - m_itemwidth) / 2;
			}
			if (m_item_alignment & itemHorizontalAlignJustify)
			{
				width -= (x * 2);
			}
			else if (m_x_itemSpace)
			{
				width = m_x_itemSpace;
				if (m_style.m_selection_width != m_itemwidth)
					width -= ((m_style.m_selection_width - m_itemwidth) / 2);
			}
			else
				width -= (xOffset * 2);
		}
		else
		{

			if (m_style.m_selection_height != m_itemheight)
			{
				y += (m_style.m_selection_height - m_itemheight) / 2;
			}

			if (m_item_alignment & itemVertialAlignJustify)
			{
				height -= (y * 2);
			}
			else if (m_y_itemSpace)
			{
				height = m_y_itemSpace;
				if (m_style.m_selection_height != m_itemheight)
					height -= (m_style.m_selection_height - m_itemheight);
			}
			else
				height -= (yOffset * 2);
		}
	}
	else if (m_scrollbar_length != 0)
	{
		x = 0;
		y = 0;
		if (m_orientation == orHorizontal)
		{
			width = m_scrollbar_length;
		}
		else
		{
			height = m_scrollbar_length;
		}
	}
	else
	{
		x = 0;
		y = 0;
	}

	if (m_scrollbar_mode == showTopAlways || m_scrollbar_mode == showTopOnDemand)
	{
		m_scrollbar->move(ePoint(x, y));
		m_scrollbar->resize(eSize(width, m_scrollbar_height));
	}
	else if (m_scrollbar_mode == showLeftOnDemand || m_scrollbar_mode == showLeftAlways)
	{
		m_scrollbar->move(ePoint(x, y));
		m_scrollbar->resize(eSize(m_scrollbar_width, height));
	}
	else if (m_orientation == orHorizontal)
	{
		m_scrollbar->move(ePoint(x, height - m_scrollbar_height));
		m_scrollbar->resize(eSize(width, m_scrollbar_height));
	}
	else
	{
		m_scrollbar->move(ePoint(width - m_scrollbar_width, y));
		m_scrollbar->resize(eSize(m_scrollbar_width, height));
	}

	if (m_orientation == orHorizontal)
		return width;
	else
		return height;
}

void eListbox::updateScrollBar()
{
	if (!m_scrollbar || !m_content || m_scrollbar_mode == showNever)
		return;
	int entries = m_content->size();
	if ((m_orientation == orGrid) && m_max_columns)
		entries = (m_content->size() + m_max_columns - 1) / m_max_columns;
	bool scrollbarvisible = m_scrollbar->isVisible();
	bool scrollbarvisibleOld = m_scrollbar->isVisible();
	int maxItems = (m_orientation == orHorizontal) ? m_max_columns : m_max_rows;

	if (m_content_changed)
	{
		int width = size().width();
		int height = size().height();

		m_content_changed = false;
		if (m_scrollbar_mode == showTopAlways || m_scrollbar_mode == showTopOnDemand)
		{
			m_content->setSize(eSize(m_itemwidth, height - m_scrollbar_height - m_scrollbar_offset));
			m_scrollbar_calcsize = setScrollbarPosition();
			if (entries > m_max_columns || m_scrollbar_mode == showLeftAlways)
			{
				m_scrollbar->show();
				scrollbarvisible = true;
			}
			else
			{
				m_scrollbar->hide();
				scrollbarvisible = false;
			}
		}
		else if (m_scrollbar_mode == showLeftOnDemand || m_scrollbar_mode == showLeftAlways)
		{
			if (m_orientation == orVertical)
				m_content->setSize(eSize(width - m_scrollbar_width - m_scrollbar_offset, m_itemheight));
			else
				m_content->setSize(eSize(m_itemwidth, m_itemheight));

			m_scrollbar_calcsize = setScrollbarPosition();

			if (entries > m_max_rows || m_scrollbar_mode == showLeftAlways)
			{
				m_scrollbar->show();
				scrollbarvisible = true;
			}
			else
			{
				m_scrollbar->hide();
				scrollbarvisible = false;
			}
		}
		else if (entries > maxItems || m_scrollbar_mode == showAlways)
		{
			if (m_orientation == orHorizontal)
			{
				m_content->setSize(eSize(m_itemwidth, height - m_scrollbar_height - m_scrollbar_offset));
			}
			else if (m_orientation == orVertical)
			{
				m_content->setSize(eSize(width - m_scrollbar_width - m_scrollbar_offset, m_itemheight));
			}
			else
			{
				m_content->setSize(eSize(m_itemwidth, m_itemheight));
			}
			m_scrollbar_calcsize = setScrollbarPosition();
			m_scrollbar->show();
			scrollbarvisible = true;
		}
		else
		{
			if (m_orientation == orHorizontal)
				m_content->setSize(eSize(m_itemwidth, height));
			else if (m_orientation == orVertical)
				m_content->setSize(eSize(width, m_itemheight));
			else
				m_content->setSize(eSize(m_itemwidth, m_itemheight));
			m_scrollbar->hide();
			scrollbarvisible = false;
		}

		if (m_scrollbar_scroll == byLine)
		{
			m_scrollbar->setRange(0, m_scrollbar_calcsize - (m_scrollbar_border_width * 2));
		}
	}

	// Don't set Start/End if scollbar not visible or entries/maxItems = 0
	if (maxItems && entries && scrollbarvisible)
	{

		if (m_scrollbar_scroll == byLine)
		{

			if (m_prev_scrollbar_page != m_selected)
			{
				m_prev_scrollbar_page = m_selected;
				int start = 0;
				int selected = (m_orientation == orGrid && m_max_columns > 0) ? m_selected / m_max_columns : m_selected;
				int range = m_scrollbar_calcsize - (m_scrollbar_border_width * 2);
				int end = range;
				// calculate thumb only if needed
				if (entries > 1 && entries > maxItems)
				{
					float fthumb = (float)maxItems / (float)entries * range;
					float fsteps = ((float)(range - fthumb) / (float)entries);
					float fstart = (float)selected * fsteps;
					fthumb = (float)range - (fsteps * (float)(entries - 1));
					int visblethumb = fthumb < 4 ? 4 : (int)(fthumb + 0.5);
					start = (int)(fstart + 0.5);
					end = start + visblethumb;
					if (end > range)
					{
						end = range;
						start = range - visblethumb;
					}
				}

				m_scrollbar->setStartEnd(start, end, true);
			}
			if (scrollbarvisible != scrollbarvisibleOld)
				recalcSizeAlignment(scrollbarvisible);
			return;
		}

		int topLeft = (m_orientation == orHorizontal) ? m_left : m_top;
		int curVisiblePage = topLeft / maxItems;

		if (m_prev_scrollbar_page != curVisiblePage)
		{
			m_prev_scrollbar_page = curVisiblePage;
			int pages = entries / maxItems;
			if ((pages * maxItems) < entries)
				++pages;
			int start = (topLeft * 100) / (pages * maxItems);
			int vis = (maxItems * 100 + pages * maxItems - 1) / (pages * maxItems);
			if (vis < 3)
				vis = 3;
			m_scrollbar->setStartEnd(start, start + vis);
		}
	}
	if (scrollbarvisible != scrollbarvisibleOld)
		recalcSizeAlignment(scrollbarvisible);
}

int eListbox::getEntryTop()
{
	/*
		Please Note! This will currently only work for verticial list box.
		It's only used in eListboxPythonMultiContent::setSelectionClip.
	*/
	if (m_orientation == orHorizontal)
		return (m_selected - m_left) * m_itemwidth;
	else if (m_orientation == orVertical)
		return (m_selected - m_top) * m_itemheight;
	else
		return (m_selected - m_top) * m_itemheight;
}

int eListbox::event(int event, void *data, void *data2)
{
	switch (event)
	{
	case evtPaint:
	{
		ePtr<eWindowStyle> style;

		if (!m_content)
			return eWidget::event(event, data, data2);
		ASSERT(m_content);

		getStyle(style);

		if (!m_content)
			return 0;

		gPainter &painter = *(gPainter *)data2;
		gRegion entryRect;
		m_content->cursorSave();
		if (m_orientation == orVertical)
			m_content->cursorMove(m_top - m_selected);
		else if (m_orientation == orHorizontal)
			m_content->cursorMove(m_left - m_selected);
		else
			m_content->cursorMove((m_max_columns * m_top) - m_selected);

		const gRegion &paint_region = *(gRegion *)data;

		if (!isTransparent())
		{
			int cornerRadius = getCornerRadius();
			int cornerRadiusEdges = getCornerRadiusEdges();
			painter.clip(paint_region);
			style->setStyle(painter, eWindowStyle::styleListboxNormal);
			if (m_style.is_set.spacing_color)
			{
				painter.setBackgroundColor(m_style.m_spacing_color);
			}
			else
			{
				if (m_style.is_set.background_color)
					painter.setBackgroundColor(m_style.m_background_color);
			}
			if (cornerRadius && cornerRadiusEdges)
			{
				painter.setRadius(cornerRadius, cornerRadiusEdges);
				painter.drawRectangle(eRect(ePoint(0, 0), size()));
			}
			else
				painter.clear();
			painter.clippop();
		}

		// --- Add support for page transition animation ---
		if (m_page_transition_active) {
			int total_offset = (m_itemheight + m_spacing.y()) * m_max_rows;
			drawPage(painter, paint_region, m_page_transition_offset, -1);
			drawPage(painter, paint_region, m_page_transition_offset - total_offset, m_top + m_page_transition_direction);
		} else {
			drawPage(painter, paint_region, 0, -1);
		}

		// scrollbar background handling (unchanged)
		if (m_scrollbar && !isTransparent())
		{
			if (m_scrollbar_mode == showLeftOnDemand || m_scrollbar_mode == showLeftAlways)
			{
				style->setStyle(painter, eWindowStyle::styleListboxNormal);
				if (m_scrollbar->isVisible())
				{
					painter.clip(eRect(m_scrollbar->position() + ePoint(m_scrollbar->size().width(), 0), eSize(m_scrollbar_offset, m_scrollbar->size().height())));
				}
				else
				{
					painter.clip(eRect(m_scrollbar->position(), eSize(m_scrollbar->size().width() + m_scrollbar_offset, m_scrollbar->size().height())));
				}

				if (m_style.is_set.spacing_color)
					painter.setBackgroundColor(m_style.m_spacing_color);
				else if (m_style.is_set.background_color)
					painter.setBackgroundColor(m_style.m_background_color);
				painter.clear();
				painter.clippop();
			}
			else if (m_scrollbar_mode == showTopOnDemand || m_scrollbar_mode == showTopAlways)
			{
				style->setStyle(painter, eWindowStyle::styleListboxNormal);
				if (m_scrollbar->isVisible())
				{
					painter.clip(eRect(m_scrollbar->position() + ePoint(0, m_scrollbar->size().height()), eSize(m_scrollbar->size().width(), m_scrollbar_offset)));
				}
				else
				{
					painter.clip(eRect(m_scrollbar->position(), eSize(m_scrollbar->size().width(), m_scrollbar->size().height() + m_scrollbar_offset)));
				}
				if (m_style.is_set.spacing_color)
					painter.setBackgroundColor(m_style.m_spacing_color);
				else if (m_style.is_set.background_color)
					painter.setBackgroundColor(m_style.m_background_color);
				painter.clear();
				painter.clippop();
			}
			else if (m_scrollbar->isVisible())
			{
				style->setStyle(painter, eWindowStyle::styleListboxNormal);
				if (m_orientation == orHorizontal)
				{
					painter.clip(eRect(m_scrollbar->position() - ePoint(0, m_scrollbar_offset), eSize(m_scrollbar->size().width(), m_scrollbar_offset)));
				}
				else
				{
					painter.clip(eRect(m_scrollbar->position() - ePoint(m_scrollbar_offset, 0), eSize(m_scrollbar_offset, m_scrollbar->size().height())));
				}
				if (m_style.is_set.spacing_color)
					painter.setBackgroundColor(m_style.m_spacing_color);
				else if (m_style.is_set.background_color)
					painter.setBackgroundColor(m_style.m_background_color);
				painter.clear();
				painter.clippop();
			}
		}

		return 0;
	}
	case evtChangedSize:
		recalcSize();
		return eWidget::event(event, data, data2);

	case evtAction:
		if (isVisible() && !isLowered())
		{
			moveSelection((long)data2);
			return 1;
		}
		return 0;

	default:
		return eWidget::event(event, data, data2);
	}
}


void eListbox::recalcSize()
{
	m_content_changed = true;
	m_prev_scrollbar_page = -1;

	bool scrollbarVisible = false;
	int xscrollBar = 0;
	[[maybe_unused]]int yscrollBar = 0;
	if (m_content)
	{
		scrollbarVisible = m_scrollbar && m_scrollbar->isVisible();
		if (scrollbarVisible)
		{
			xscrollBar = (m_orientation == orGrid) ? (m_scrollbar->size().width() + m_scrollbar_offset) : 0;
			yscrollBar = (m_orientation == orHorizontal) ? (m_scrollbar->size().height() + m_scrollbar_offset) : 0;
		}
	}

	if (m_orientation == orVertical)
	{
		m_style.m_selection_height = m_itemheight;
		m_itemwidth_set = false; // reset m_itemwidth
		if (size().width() > -1)
		{
			m_itemwidth = size().width();
			m_style.m_selection_width = m_itemwidth;
		}
		if (m_content)
			m_content->setSize(eSize(m_itemwidth, m_itemheight));
		m_max_rows = (size().height() - m_spacing.y() * 2) / (m_itemheight + m_spacing.y() * 2);
	}
	else if (m_orientation == orHorizontal)
	{
		if (size().height() > -1 && !m_itemheight_set)
		{
			m_style.m_selection_zoom = 1.0;
			m_itemheight = size().height();
			m_style.m_selection_height = m_itemheight;
		}
		if (m_content)
			m_content->setSize(eSize(m_itemwidth, m_itemheight));
		int w = size().width();
		m_max_columns = w / (m_itemwidth + m_spacing.x());
		if (m_style.m_selection_zoom > 1.0)
		{
			int item_w_zoom = (m_style.m_selection_width) - m_spacing.x();
			if (m_max_columns > 1)
			{
				if (w < (item_w_zoom + (m_max_columns - 1) * (m_itemwidth + m_spacing.x())))
					m_max_columns--;
			}
		}
	}
	else
	{
		if (m_content)
			m_content->setSize(eSize(m_itemwidth, m_itemheight));

		int w = size().width() - xscrollBar;
		int h = size().height();

		m_max_columns = w / (m_itemwidth + m_spacing.x());
		m_max_rows = h / (m_itemheight + m_spacing.y());

		if (m_style.m_selection_zoom > 1.0)
		{

			int item_w_zoom = m_style.m_selection_width - m_spacing.x();
			int item_h_zoom = m_style.m_selection_height - m_spacing.y();

			if (m_max_columns > 1)
			{
				if (w < (item_w_zoom + (m_max_columns - 1) * (m_itemwidth + m_spacing.x())))
					m_max_columns--;
			}

			if (m_max_rows > 1)
			{
				if (h < (item_h_zoom + (m_max_rows - 1) * (m_itemheight + m_spacing.y())))
					m_max_rows--;
			}
		}
	}

	/* TODO: whyever - our size could be invalid, or itemheigh could be wrongly specified. */
	if (m_max_columns < 0)
		m_max_columns = 0;
	if (m_max_rows < 0)
		m_max_rows = 0;

	if (m_content)
		recalcSizeAlignment(scrollbarVisible);


	moveSelection(justCheck);
}

void eListbox::recalcSizeAlignment(bool scrollbarVisible)
{

	if (m_orientation != orVertical && m_item_alignment != itemAlignLeftTop)
	{

		int xscrollBar = (m_orientation == orGrid) ? ((scrollbarVisible) ? m_scrollbar->size().width() + m_scrollbar_offset : 0) : 0;
		int yscrollBar = (m_orientation == orHorizontal) ? ((scrollbarVisible) ? m_scrollbar->size().height() + m_scrollbar_offset : 0) : 0;
		int xfullSpace = size().width() - xscrollBar;
		int yfullSpace = size().height() - yscrollBar;
		m_x_itemSpace = m_style.m_selection_width + m_defined_spacing.x();
		if (m_max_columns > 1)
		{
			m_x_itemSpace += ((m_max_columns - 1) * (m_itemwidth + m_defined_spacing.x()));

			if (m_style.m_selection_width == m_itemwidth) // no zoom : remove 1 space
				m_x_itemSpace -= m_defined_spacing.x();
			else
			{ // zoom : remove 0.5 delta of zoom
				m_x_itemSpace -= ((m_style.m_selection_width - m_itemwidth) / 2);
			}
		}

		m_y_itemSpace = m_style.m_selection_height + m_defined_spacing.y();
		if (m_max_rows > 1)
		{
			m_y_itemSpace += ((m_max_rows - 1) * (m_itemheight + m_defined_spacing.y()));
			if (m_style.m_selection_height == m_itemheight) // no zoom : remove 1 space
				m_y_itemSpace -= m_defined_spacing.y();
			else
			{ // zoom : remove 0.5 delta of zoom
				m_y_itemSpace -= ((m_style.m_selection_height - m_itemheight) / 2);
			}
		}

		int scrollbarLeftSpace = (m_scrollbar_mode == showLeftOnDemand || m_scrollbar_mode == showLeftAlways) ? xscrollBar : 0;
		int scrollbarTopSpace = (m_scrollbar_mode == showTopOnDemand || m_scrollbar_mode == showTopAlways) ? yscrollBar : 0;

		m_spacing = m_defined_spacing;

		if (xfullSpace > m_x_itemSpace)
		{
			xOffset = scrollbarLeftSpace;
			if (m_item_alignment & itemHorizontalAlignCenter)
				xOffset = ((xfullSpace - m_x_itemSpace) / 2) + scrollbarLeftSpace;
			if (m_item_alignment & itemHorizontalAlignRight)
				xOffset = (xfullSpace - m_x_itemSpace) + scrollbarLeftSpace;
			if (m_item_alignment & itemHorizontalAlignJustify)
			{
				m_x_itemSpace = m_style.m_selection_width + ((m_max_columns - 1) * m_itemwidth);
				int xspace = (xfullSpace - m_x_itemSpace) / (m_max_columns - 1);
				m_spacing.setX(xspace);
			}
		}
		if (yfullSpace > m_y_itemSpace)
		{
			yOffset = scrollbarTopSpace;
			if (m_item_alignment & itemVertialAlignMiddle)
				yOffset = ((yfullSpace - m_y_itemSpace) / 2) + scrollbarTopSpace;
			if (m_item_alignment & itemVertialAlignBottom)
				yOffset = (yfullSpace - m_y_itemSpace) + scrollbarTopSpace;
			if (m_item_alignment & itemVertialAlignJustify)
			{
				m_y_itemSpace = m_style.m_selection_height + ((m_max_rows - 1) * m_itemheight);
				int yspace = (yfullSpace - m_y_itemSpace) / (m_max_rows - 1);
				m_spacing.setY(yspace);
			}
		}
	}

	if (m_scrollbar)
	{
		if(m_orientation != orVertical)
		{
			if (m_scrollbar_mode == showTopOnDemand || m_scrollbar_mode == showTopAlways)
			{
				yOffset = m_scrollbar->size().height() + m_scrollbar_offset;
			}
		}
		else {
			if (m_scrollbar_mode == showLeftOnDemand || m_scrollbar_mode == showLeftAlways)
			{
				xOffset = m_scrollbar->size().width() + m_scrollbar_offset;
			}
		}
	}
}

void eListbox::setItemHeight(int h)
{
	if (h && m_itemheight != h)
	{
		m_itemheight = h;
		m_itemheight_set = true;
		m_style.m_selection_height = h;
		recalcSize();
	}
}

void eListbox::setItemWidth(int w)
{
	if (w && m_itemwidth != w)
	{
		m_itemwidth = w;
		m_itemwidth_set = true;
		m_style.m_selection_width = w;
		recalcSize();
	}
}

void eListbox::setSelectionEnable(int en)
{
	if (m_selection_enabled == en)
		return;
	m_selection_enabled = en;
	entryChanged(m_selected); /* redraw current entry */
}

void eListbox::entryAdded(int index)
{
	m_first_selectable_item = -1;
	m_last_selectable_item = -1;
	if (m_content)
	{
		if (m_orientation == orVertical)
		{
			if ((m_content->size() % m_max_rows) == 1)
				m_content_changed = true;
		}
		else if (m_orientation == orHorizontal)
		{
			if ((m_content->size() % m_max_columns) == 1)
				m_content_changed = true;
		}
		else
			m_content_changed = true;
	}

	/* manage our local pointers. when the entry was added before the current position, we have to advance. */
	/* we need to check <= - when the new entry has the (old) index of the cursor, the cursor was just moved down. */

	if (index <= m_selected)
		++m_selected;
	if (m_orientation == orVertical)
	{
		if (index <= m_top)
			++m_top;
	}
	else
	{
		if (index <= m_left)
			++m_left;
	}

	/* we have to check wether our current cursor is gone out of the screen. */
	/* moveSelection will check for this case */
	moveSelection(justCheck);

	/* now, check if the new index is visible. */
	if (m_orientation == orVertical)
	{
		if ((m_top <= index) && (index < (m_top + m_max_rows)))
		{
			/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else if (m_orientation == orHorizontal)
	{
		if ((m_left <= index) && (index < (m_left + m_max_columns)))
		{
			/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else
		invalidate();
}

void eListbox::entryRemoved(int index)
{
	m_first_selectable_item = -1;
	m_last_selectable_item = -1;

	if (m_content)
	{
		if (m_orientation == orVertical)
		{
			if (!(m_content->size() % m_max_rows))
				m_content_changed = true;
		}
		else if (m_orientation == orHorizontal)
		{
			if (!(m_content->size() % m_max_columns))
				m_content_changed = true;
		}
		else
			m_content_changed = true;
	}

	if (index == m_selected && m_content)
		m_selected = m_content->cursorGet();

	if (m_content && m_content->cursorGet() >= m_content->size())
		moveSelection(moveUp);
	else
		moveSelection(justCheck);

	if (m_orientation == orVertical)
	{
		if ((m_top <= index) && (index < (m_top + m_max_rows)))
		{
			/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else if (m_orientation == orHorizontal)
	{
		if ((m_left <= index) && (index < (m_left + m_max_columns)))
		{
			/* todo, calc exact invalidation... */
			invalidate();
		}
	}
	else
		invalidate();
}

void eListbox::entryChanged(int index)
{
	gRegion inv = eRect(getItemPostion(index), eSize(m_itemwidth, m_itemheight));
	invalidate(inv);
}

void eListbox::entryReset(bool selectionHome)
{
	m_first_selectable_item = -1;
	m_last_selectable_item = -1;
	m_content_changed = true;
	m_prev_scrollbar_page = -1;
	int oldSel;

	if (selectionHome)
	{
		if (m_content)
			m_content->cursorHome();
		m_top = 0;
		m_left = 0;
		m_selected = 0;
	}

	if (m_content && (m_selected >= m_content->size()))
	{
		if (m_content->size())
			m_selected = m_content->size() - 1;
		else
			m_selected = 0;
		m_content->cursorSet(m_selected);
	}

	oldSel = m_selected;
	moveSelection(justCheck);
	/* if oldSel != m_selected, selectionChanged was already
	   emitted in moveSelection. we want it in any case, so otherwise,
	   emit it now. */
	if (oldSel == m_selected)
		/* emit */ selectionChanged();
	invalidate();
}

void eListbox::setSpacingColor(const gRGB &col)
{
	eWidget::setBackgroundColor(col);
	m_style.m_spacing_color = col;
	m_style.is_set.spacing_color = 1;
}

void eListbox::setBackgroundColor(const gRGB &col)
{
	m_style.m_background_color = col;
	m_style.is_set.background_color = 1;
}

void eListbox::setBackgroundColorSelected(const gRGB &col)
{
	m_style.m_background_color_selected = col;
	m_style.is_set.background_color_selected = 1;
}

void eListbox::setBackgroundColorRows(const gRGB &col)
{
	m_style.m_background_color_rows = col;
	m_style.is_set.background_color_rows = 1;
}

void eListbox::setForegroundColor(const gRGB &col)
{
	m_style.m_foreground_color = col;
	m_style.is_set.foreground_color = 1;
}

void eListbox::setForegroundColorSelected(const gRGB &col)
{
	m_style.m_foreground_color_selected = col;
	m_style.is_set.foreground_color_selected = 1;
}

void eListbox::setBorderWidth(int width)
{
	m_style.m_border_size = width;
	if (m_scrollbar)
		m_scrollbar->setBorderWidth(width);
}

void eListbox::setScrollbarBorderWidth(int width)
{
	m_style.m_scrollbarborder_width = width;
	m_style.is_set.scrollbarborder_width = 1;
	if (m_scrollbar)
		m_scrollbar->setBorderWidth(width);
}

void eListbox::setScrollbarForegroundPixmap(ePtr<gPixmap> &pm)
{
	m_scrollbarpixmap = pm;
	if (m_scrollbar && m_scrollbarpixmap)
		m_scrollbar->setPixmap(pm);
}

void eListbox::setScrollbarBackgroundColor(gRGB &col)
{
	m_style.m_scrollbarbackground_color = col;
	m_style.is_set.scrollbarbackground_color = 1;
	if (m_scrollbar)
		m_scrollbar->setBackgroundColor(col);
}

void eListbox::setScrollbarForegroundColor(gRGB &col)
{
	m_style.m_scrollbarforeground_color = col;
	m_style.is_set.scrollbarforeground_color = 1;
	if (m_scrollbar)
		m_scrollbar->setForegroundColor(col);
}

void eListbox::setScrollbarBorderColor(const gRGB &col)
{
	m_style.m_scollbarborder_color = col;
	m_style.is_set.scollbarborder_color = 1;
	if (m_scrollbar)
		m_scrollbar->setBorderColor(col);
}

void eListbox::setScrollbarBackgroundPixmap(ePtr<gPixmap> &pm)
{
	m_scrollbarbackgroundpixmap = pm;
	if (m_scrollbar && m_scrollbarbackgroundpixmap)
		m_scrollbar->setBackgroundPixmap(pm);
}

void eListbox::setScrollbarForegroundGradient(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	m_style.m_scrollbarforegroundgradient_colors = {startcolor, midcolor, endcolor};
	m_style.is_set.scrollbarforegroundgradient = 1;
	if (m_scrollbar)
		m_scrollbar->setForegroundGradient(m_style.m_scrollbarforegroundgradient_colors, (m_orientation == orHorizontal) ? 2 : 1, false, true);
}

void eListbox::setScrollbarBackgroundGradient(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	m_style.m_scrollbarbackgroundgradient_colors = {startcolor, midcolor, endcolor};
	m_style.is_set.scrollbarbackgroundgradient = 1;
	if (m_scrollbar)
		m_scrollbar->setBackgroundGradient(m_style.m_scrollbarbackgroundgradient_colors, (m_orientation == orHorizontal) ? 2 : 1, false);
}

void eListbox::setScrollbarRadius(int radius, uint8_t edges)
{
	m_style.m_scrollbar_radius = radius;
	m_style.m_scrollbar_edges = edges;
	if (m_scrollbar)
		m_scrollbar->setCornerRadius(radius, edges);
}

void eListbox::setItemAlignment(int align)
{
	if (m_item_alignment != align)
	{
		m_item_alignment = align;
		invalidate();
	}
}

void eListbox::invalidate(const gRegion &region)
{
	gRegion tmp(region);
	if (m_content)
		m_content->updateClip(tmp);
	eWidget::invalidate(tmp);
}

struct eListboxStyle *eListbox::getLocalStyle(void)
{
	/* transparency is set directly in the widget */
	m_style.is_set.transparent_background = isTransparent();
	return &m_style;
}

void eListbox::setOrientation(uint8_t newOrentation)
{
	if (m_orientation != newOrentation)
	{
		if (m_scrollbar)
		{
			if (newOrentation == orHorizontal)
			{
				m_scrollbar->setOrientation(eSlider::orHorizontal);
			}
			else
			{
				m_scrollbar->setOrientation(eSlider::orVertical);
			}
		}
		m_orientation = newOrentation;
		recalcSize();
	}
	invalidate();
}

void eListbox::setItemSpacing(const ePoint &spacing, bool innerOnly)
{
	if ((spacing.x() >= 0 && spacing.y() >= 0))
	{
		m_spacing = spacing;
		m_defined_spacing = spacing;
		m_spacing_innerOnly = innerOnly;
		recalcSize();
		invalidate();
	}
}

void eListbox::setFont(gFont *font)
{
	m_style.m_font = font;
	if (m_style.m_selection_zoom > 1.0)
		m_style.m_font_zoomed = new gFont(m_style.m_font->family, m_style.m_font->pointSize * m_style.m_selection_zoom);
}

void eListbox::setSelectionZoom(float zoom, int zoomContentMode)
{
	if (zoom > 1.0)
	{
		m_style.m_selection_zoom = zoom;
		if (m_style.m_font)
			m_style.m_font_zoomed = new gFont(m_style.m_font->family, m_style.m_font->pointSize * zoom);

		m_style.m_selection_width = m_itemwidth * zoom;
		m_style.m_selection_height = m_itemheight * zoom;

		m_style.is_set.zoom_content = zoomContentMode == zoomContentZoom;
		m_style.is_set.zoom_move_content = zoomContentMode == zoomContentMove;

		recalcSize();
		invalidate();
	}
}

void eListbox::setSelectionZoomSize(int width, int height, int zoomContentMode)
{

	if (m_orientation != orVertical && m_itemwidth && m_itemheight && width > m_itemwidth && height > m_itemheight)
	{

		m_style.m_selection_width = width;
		m_style.m_selection_height = height;

		m_style.m_selection_zoom = (float)width / (float)m_itemwidth;
		m_style.is_set.zoom_content = zoomContentMode == zoomContentZoom;
		m_style.is_set.zoom_move_content = zoomContentMode == zoomContentMove;

		if (m_style.m_selection_zoom > 1.0 && m_style.m_font)
			m_style.m_font_zoomed = new gFont(m_style.m_font->family, m_style.m_font->pointSize * m_style.m_selection_zoom);

		recalcSize();
		invalidate();
	}
}

ePoint eListbox::getItemPostion(int index)
{
    int posx = 0, posy = 0;
    ePoint spacing = (index > 0) ? m_spacing : ePoint(0, 0);

    if (m_orientation == orGrid) {
        posx = (m_itemwidth + spacing.x()) * ((index - (m_top * m_max_columns)) % m_max_columns);
        posy = (m_itemheight + spacing.y()) * ((index - (m_top * m_max_columns)) / m_max_columns);
    } else if (m_orientation == orHorizontal) {
        posx = (m_itemwidth + spacing.x()) * (index - m_left);
    } else {
        posy = (m_itemheight + spacing.y()) * (index - m_top);
    }

    return ePoint(posx + xOffset, posy + yOffset);
}
void eListbox::onScrollTimer()
{
    if (!m_page_transition_active)
    {
        m_scroll_timer->stop();
        return;
    }

    const int total_offset = (m_itemheight + m_spacing.y()) * m_max_rows;
    const float ease = 0.25f;

    int target = m_page_transition_direction * total_offset;
    int remaining = target - m_page_transition_offset;
    int step = static_cast<int>(remaining * ease);

    if (std::abs(step) < 1)
        step = (remaining > 0) ? 1 : -1;

    m_page_transition_offset += step;
    invalidate();

    if (std::abs(target - m_page_transition_offset) < 1)
    {
        // Animation done
        m_page_transition_offset = 0;
        m_page_transition_active = false;
        m_scroll_timer->stop();

        if (m_page_transition_direction == 1)
        {
            // Update m_top only after animation completes
            if (m_top < 0) // Handle wraparound case
            {
                int maxRows = (m_content->size() + m_max_columns - 1) / m_max_columns;
                m_top = maxRows - m_max_rows;
                if (m_top < 0) m_top = 0; // Ensure non-negative
            }
            else
            {
                m_top++; // Move to next page
            }
            eDebug("[eListbox] onScrollTimer: animation complete, m_top=%d", m_top);
        }
        else if (m_page_transition_direction == -1 && m_top > 0)
        {
            m_top--; // Move to previous page
            eDebug("[eListbox] onScrollTimer: animation complete, m_top=%d", m_top);
        }

        // Keep m_selected unchanged to maintain cursor position
        m_content_changed = true;
        updateScrollBar();
        invalidate();
    }
    else
    {
        m_scroll_timer->start(16, true);
    }
}

void eListbox::drawPage(gPainter &painter, const gRegion &paint_region, int offsetY, int topOverride /* = -1 */)
{
    ePtr<eWindowStyle> style;
    getStyle(style);

    int savedTop = m_top;
    if (topOverride != -1)
        m_top = topOverride;

    int startIndex, endIndex;
    if (m_orientation == orGrid) {
        startIndex = m_top * m_max_columns;
        endIndex = std::min((int)m_content->size(), startIndex + m_max_columns * m_max_rows);
        if (m_content->size() > 0 && startIndex >= m_content->size()) {
            startIndex = std::max(0, ((m_content->size() + m_max_columns - 1) / m_max_columns - m_max_rows) * m_max_columns);
            m_top = startIndex / m_max_columns; // Adjust m_top for grid
            eDebug("[eListbox] drawPage: adjusted m_top=%d for grid to prevent overflow", m_top);
        }
    } else {
        startIndex = m_top;
        endIndex = std::min((int)m_content->size(), m_top + m_max_rows);
        if (m_content->size() > 0 && startIndex >= m_content->size()) {
            startIndex = std::max(0, (int)m_content->size() - m_max_rows);
            m_top = startIndex; // Adjust m_top for vertical
            eDebug("[eListbox] drawPage: adjusted m_top=%d for vertical to prevent overflow", m_top);
        }
    }

    m_content->cursorSet(startIndex);

    for (int i = startIndex; i < endIndex; ++i)
    {
        ePoint pos = getItemPostion(i);
        if (m_orientation == orGrid && i == m_selected && m_page_transition_active) {
            // Skip offsetY for selected item to keep cursor fixed
            // Position is already correct from getItemPostion relative to m_top
            eDebug("[eListbox] drawPage: drawing selected item %d at fixed pos (%d, %d)", i, pos.x(), pos.y());
        } else {
            pos.setY(pos.y() - offsetY); // Apply animation offset to non-selected items
            eDebug("[eListbox] drawPage: drawing item %d at offset pos (%d, %d)", i, pos.x(), pos.y() - offsetY);
        }

        bool sel = (i == m_selected && m_selection_enabled);

        m_content->paint(painter, *style, pos, sel);
        m_content->cursorMove(+1);
    }

    // Restore the original top and cursor
    m_top = savedTop;
    m_content->cursorSet(m_selected);
}

void eListbox::moveSelection(int dir)
{
	/* refuse to do anything without a valid list. */
	if (!m_content)
		return;
	/* if our list does not have one entry, don't do anything. */
	int maxItems = (m_orientation == orVertical) ? m_max_rows : m_max_columns;
	if (m_orientation == orGrid) {
		maxItems = m_max_rows * m_max_columns;
	}

	if (!maxItems || !m_content->size())
		return;

	if (dir == refresh)
	{
		invalidate();
		return;
	}
	// patch pageUp / pageDown for virtual listbox if native keys enabled
	if (m_orientation == orVertical && m_native_keys_bound)
	{
		if (dir == moveLeft)
			dir = movePageUp;
		if (dir == moveRight)
			dir = movePageDown;
	}

	bool isGrid = m_orientation == orGrid;

	/* we need the old top/sel to see what we have to redraw */
	int oldTop = m_top;
	int oldLeft = m_left;
	int oldSel = m_selected;
	int prevSel = oldSel;
	int newSel;
	int pageOffset = (m_page_size > 0 && m_scrollbar_scroll == byLine) ? m_page_size : maxItems;
	int oldRow = (isGrid && m_max_columns != 0) ? oldSel / m_max_columns : 0;
	int oldColumn = (isGrid && m_max_columns != 0) ? oldSel % m_max_columns : 0;

	bool indexChanged = dir > 100;
	if (indexChanged)
		dir -= 100;

#ifdef USE_LIBVUGLES2
	m_dir = dir;
#endif

	// Injected smooth scroll + wraparound for Horizontal listbox
	if (m_orientation == orHorizontal && (dir == moveRight || dir == moveLeft))
	{
		int delta = (dir == moveRight) ? 1 : -1;
		m_content->cursorMove(delta);
		newSel = m_content->cursorGet();

		if (newSel == oldSel)
		{
			if (m_enabled_wrap_around)
			{
				if (dir == moveRight)
					m_content->cursorHome();
				else
				{
					m_content->cursorEnd();
					m_content->cursorMove(-1);
				}
				newSel = m_content->cursorGet();
				if (newSel == oldSel)
					return;
			}
			else
			{
				return;
			}
		}

		m_selected = newSel;

		m_scroll_direction = dir;
		m_scroll_current_offset = 0;
		m_scroll_target_offset = m_itemwidth + m_spacing.x();
		m_animating_scroll = true;
		m_scroll_timer->start(16, true);
	}
	else
	{
		switch (dir)
		{
		case moveFirst:
			if (isGrid)
			{
				int newRow = oldRow;
				do
				{
					m_content->cursorMove(-1);
					newSel = m_content->cursorGet();
					newRow = newSel / m_max_columns;
					if (newRow != oldRow || !m_content->currentCursorSelectable())
					{
						m_content->cursorSet(prevSel);
						break;
					}
					if (newSel == prevSel)
					{
						break;
					}
					prevSel = newSel;
				} while (true);
			}
			break;
		case moveLast:
			if (isGrid)
			{
				int newRow = oldRow;
				do
				{
					m_content->cursorMove(1);
					newSel = m_content->cursorGet();
					newRow = newSel / m_max_columns;
					if (newRow != oldRow || !m_content->currentCursorSelectable())
					{
						m_content->cursorSet(prevSel);
						break;
					}
					if (newSel == prevSel)
					{
						break;
					}
					prevSel = newSel;
				} while (true);
			}
			break;
		case moveBottom:
			m_content->cursorEnd();
			[[fallthrough]];
		case moveUp:
			if (isGrid) {
				eDebug("[eListbox] moveUp: oldSel=%d, m_selected=%d, m_top=%d, maxRows=%d, wrap=%d", oldSel, m_selected, m_top, (m_content->size() + m_max_columns - 1) / m_max_columns, m_enabled_wrap_around);
				int maxRows = (m_content->size() + m_max_columns - 1) / m_max_columns;
				if (m_top > 0) {
					m_scroll_direction = moveDown; // Rows slide down
					m_scroll_current_offset = 0;
					m_scroll_target_offset = -(m_itemheight + m_spacing.y()); // Negative for downward slide
					m_animating_scroll = true;
					m_scroll_timer->start(16, true);
					m_top--; // Shift all rows up
					eDebug("[eListbox] moveUp: sliding down, m_top=%d, scroll_direction=%d, target_offset=%d", m_top, m_scroll_direction, m_scroll_target_offset);
					invalidate();
				} else if (m_enabled_wrap_around && maxRows > m_max_rows) {
					m_top = maxRows - m_max_rows;
					m_scroll_direction = moveDown;
					m_scroll_current_offset = 0;
					m_scroll_target_offset = -(m_itemheight + m_spacing.y());
					m_animating_scroll = true;
					m_scroll_timer->start(16, true);
					eDebug("[eListbox] moveUp: wrapped, m_top=%d, scroll_direction=%d, target_offset=%d", m_top, m_scroll_direction, m_scroll_target_offset);
					invalidate();
				} else {
					eDebug("[eListbox] moveUp: no action, at top");
				}
				break;
			}
			do {
				m_content->cursorMove(-1);
				newSel = m_content->cursorGet();
				if (newSel == oldSel) {
					if (m_enabled_wrap_around && m_content->size() > 0) {
						m_content->cursorEnd();
						m_content->cursorMove(-1);
						newSel = m_content->cursorGet();
					} else {
						m_content->cursorSet(oldSel);
						break;
					}
				}
				prevSel = newSel;
			} while (newSel != oldSel && !m_content->currentCursorSelectable());
			break;

		case moveLeft:
		{
			if (isGrid)
			{
				int firstVisibleIndex = m_top * m_max_columns;
				int lastVisibleIndex = std::min((int)m_content->size(), firstVisibleIndex + m_max_columns * m_max_rows);
		
				if (oldSel > firstVisibleIndex)
				{
					m_selected = oldSel - 1;  // Move left across items
				}
				else if (m_enabled_wrap_around)
				{
					// Wrap to last visible item on page
					m_selected = lastVisibleIndex - 1;
				}
		
				if (m_selected != oldSel)
				{
					m_content->cursorSet(m_selected); // Update cursor
					gRegion inv = eRect(getItemPostion(m_selected), eSize(m_style.m_selection_width, m_style.m_selection_height));
					inv |= eRect(getItemPostion(oldSel), eSize(m_style.m_selection_width, m_style.m_selection_height));
					invalidate(inv); // Redraw old and new
					selectionChanged(); // Notify selection change
				}
				break;
			}
		
			// Non-grid behavior (unchanged)
			do {
				m_content->cursorMove(-1);
				newSel = m_content->cursorGet();
				if (newSel == prevSel)
				{
					if (m_enabled_wrap_around)
					{
						m_content->cursorEnd();
						m_content->cursorMove(-1);
						newSel = m_content->cursorGet();
					}
					else
					{
						m_content->cursorSet(oldSel);
						break;
					}
				}
				prevSel = newSel;
			} while (newSel != oldSel && !m_content->currentCursorSelectable());
			break;
		}

		case moveTop:
			m_content->cursorHome();
			[[fallthrough]];
		case justCheck:
			if (m_content->cursorValid() && m_content->currentCursorSelectable())
				break;
			[[fallthrough]];
		case moveDown:
		{
		    int maxRows = (m_content->size() + m_max_columns - 1) / m_max_columns;
		
		    if (isGrid) {
		        eDebug("[eListbox] moveDown: oldSel=%d, m_selected=%d, m_top=%d, maxRows=%d, wrap=%d", oldSel, m_selected, m_top, maxRows, m_enabled_wrap_around);
		
		        if (m_page_transition_active) {
		            eDebug("[eListbox] moveDown: animation already active, skipping.");
		            return; // Skip new animation to prevent cursor movement
		        }
		
		        if (m_top < maxRows - m_max_rows) {
		            m_page_transition_active = true;
		            m_page_transition_direction = 1; // Slide current page up
		            m_scroll_timer->start(16, true);
		            return; // Don't change m_selected
		        }
		        else if (m_enabled_wrap_around && maxRows > m_max_rows) {
		            m_page_transition_active = true;
		            m_page_transition_direction = 1;
		            m_top = -m_max_rows; // Temporary state for visual wrap
		            m_scroll_timer->start(16, true);
		            return; // Don't change m_selected
		        }
		        else {
		            eDebug("[eListbox] moveDown: no action, at bottom");
		        }
		    }
		    else {
		        if (m_selected >= m_content->size() - 1) {
		            if (m_enabled_wrap_around && m_content->size() > 0) {
		                eDebug("[eListbox] moveDown: at last index, wrapping to first");
		                m_content->cursorHome();
		                newSel = m_content->cursorGet();
		                m_selected = newSel;
		                m_top = 0;
		                eDebug("[eListbox] moveDown: after cursorHome, newSel=%d, m_top=%d", newSel, m_top);
		                invalidate(); // Force full redraw
		                selectionChanged();
		            }
		            else {
		                eDebug("[eListbox] moveDown: at last index, no wraparound");
		                m_content->cursorSet(oldSel); // Restore cursor
		            }
		        }
		        else {
		            m_content->cursorMove(1);
		            newSel = m_content->cursorGet();
		            eDebug("[eListbox] moveDown: after cursorMove(1), newSel=%d, oldSel=%d", newSel, oldSel);
		            if (newSel != oldSel && m_content->currentCursorSelectable()) {
		                m_selected = newSel;
		                eDebug("[eListbox] moveDown: moved to newSel=%d", newSel);
		                if (m_selected >= m_top + m_max_rows) {
		                    m_top = std::min(m_selected - (m_max_rows - 1), (int)(m_content->size() - m_max_rows));
		                    if (m_top < 0) m_top = 0; // Ensure non-negative
		                    eDebug("[eListbox] moveDown: adjusted m_top=%d", m_top);
		                    invalidate(); // Force full redraw
		                }
		                selectionChanged();
		            }
		            else {
		                eDebug("[eListbox] moveDown: restoring oldSel=%d", oldSel);
		                m_content->cursorSet(oldSel);
		            }
		        }
		    }
		    break;
		}

		case moveRight:
		{
			if (isGrid)
			{
				int firstVisibleIndex = m_top * m_max_columns;
				int lastVisibleIndex = std::min((int)m_content->size(), firstVisibleIndex + m_max_columns * m_max_rows);
		
				if (oldSel + 1 < lastVisibleIndex)
				{
					m_selected = oldSel + 1;  // Move right across grid page
				}
				else if (m_enabled_wrap_around)
				{
					m_selected = firstVisibleIndex;  // Wrap to first item on page
				}
		
				if (m_selected != oldSel)
				{
					m_content->cursorSet(m_selected);  // Update content cursor
					gRegion inv = eRect(getItemPostion(m_selected), eSize(m_style.m_selection_width, m_style.m_selection_height));
					inv |= eRect(getItemPostion(oldSel), eSize(m_style.m_selection_width, m_style.m_selection_height));
					invalidate(inv);  // Redraw old and new cursor positions
					selectionChanged();  // Notify listener
				}
				break;
			}
		
			// Non-grid fallback
			do {
				m_content->cursorMove(-1);
				newSel = m_content->cursorGet();
				if (newSel == prevSel)
				{
					if (m_enabled_wrap_around)
					{
						m_content->cursorEnd();
						m_content->cursorMove(-1);
						newSel = m_content->cursorGet();
					}
					else
					{
						m_content->cursorSet(oldSel);
						break;
					}
				}
				prevSel = newSel;
			} while (newSel != oldSel && !m_content->currentCursorSelectable());
			break;
		}

		case movePageUp:
		{
			int pageind;
			do
			{
				m_content->cursorMove(-pageOffset);
				newSel = m_content->cursorGet();
				pageind = newSel % maxItems;
				prevSel = newSel - pageind;
				while (newSel != prevSel + maxItems && m_content->cursorValid() && !m_content->currentCursorSelectable())
				{
					m_content->cursorMove(1);
					newSel = m_content->cursorGet();
				}
				if (!m_content->currentCursorSelectable())
				{
					m_content->cursorSet(prevSel + pageind);
					while (newSel != prevSel && !m_content->currentCursorSelectable())
					{
						m_content->cursorMove(-1);
						newSel = m_content->cursorGet();
					}
				}
				if (m_content->currentCursorSelectable())
					break;
				if (newSel == 0)
				{
					while (newSel != oldSel && !m_content->currentCursorSelectable())
					{
						m_content->cursorMove(1);
						newSel = m_content->cursorGet();
					}
					break;
				}
				m_content->cursorSet(prevSel + pageind);
			} while (newSel == prevSel);
			break;
		}
		case movePageDown:
		{
			int pageind;
			do
			{
				m_content->cursorMove(pageOffset);
				if (!m_content->cursorValid())
					m_content->cursorMove(-1);
				newSel = m_content->cursorGet();
				pageind = newSel % maxItems;
				prevSel = newSel - pageind;
				while (newSel != prevSel && !m_content->currentCursorSelectable())
				{
					m_content->cursorMove(-1);
					newSel = m_content->cursorGet();
				}
				if (!m_content->currentCursorSelectable())
				{
					m_content->cursorSet(prevSel + pageind);
					do
					{
						m_content->cursorMove(1);
						newSel = m_content->cursorGet();
					} while (newSel != prevSel + maxItems && m_content->cursorValid() && !m_content->currentCursorSelectable());
				}
				if (!m_content->cursorValid())
				{
					do
					{
						m_content->cursorMove(-1);
						newSel = m_content->cursorGet();
					} while (newSel != oldSel && !m_content->currentCursorSelectable());
					break;
				}
				if (newSel != prevSel + maxItems)
					break;
				m_content->cursorSet(prevSel + pageind);
			} while (newSel == prevSel + maxItems);
			break;
		}
		}
		m_selected = m_content->cursorGet();
	}

	if (m_orientation == orHorizontal)
		m_left = m_selected - (m_selected % maxItems);
	else
		m_top = (m_scrollbar_scroll == byLine) ? m_selected / maxItems : (m_selected / maxItems) * m_max_rows;

	if (m_scrollbar_scroll == byLine && m_content->size() > maxItems)
	{
		if (m_orientation == orHorizontal)
			m_left = moveSelectionLineMode((dir == moveLeft), (dir == moveRight), dir, oldSel, oldLeft, oldRow, maxItems, indexChanged, pageOffset, m_left);
		else
		{
			if(m_orientation == orGrid && indexChanged)
			{
				int newline = (m_selected / m_max_columns);
				m_top = std::max(newline - ((m_max_rows + 1) / 2) + 1, 0);
			}
			else
				m_top = moveSelectionLineMode((dir == moveUp), (dir == moveDown), dir, oldSel, oldTop, oldRow, maxItems, indexChanged, pageOffset, m_top);
		}
	}

	if (m_orientation == orHorizontal)
	{
		if (m_left != oldLeft && m_content)
			m_content->resetClip();
	}
	else
	{
		if (m_top != oldTop && m_content)
			m_content->resetClip();
	}

	if (oldSel != m_selected) /* emit */
		selectionChanged();

	updateScrollBar();

	if (m_orientation == orHorizontal)
	{
		if (m_left != oldLeft)
		{
			invalidate();
		}
		else if (m_selected != oldSel)
		{
			gRegion inv = eRect(getItemPostion(m_selected), eSize((m_style.m_selection_width), (m_style.m_selection_height)));
			inv |= eRect(getItemPostion(oldSel), eSize((m_style.m_selection_width), (m_style.m_selection_height)));
			invalidate(inv);
		}
	}
	else
	{
		if (m_top != oldTop)
		{
			invalidate();
		}
		else if (m_selected != oldSel)
		{
			gRegion inv = eRect(getItemPostion(m_selected), eSize((m_style.m_selection_width), (m_style.m_selection_height)));
			inv |= eRect(getItemPostion(oldSel), eSize((m_style.m_selection_width), (m_style.m_selection_height)));
			invalidate(inv);
		}
	}
}


int eListbox::moveSelectionLineMode(bool doUp, bool doDown, int dir, int oldSel, int oldTopLeft, int oldRow, int maxItems, bool indexChanged, int pageOffset, int topLeft)
{
	int oldLine = m_content->cursorRestoreLine();
	int max = m_content->size() - maxItems;
	bool customPageSize = pageOffset != maxItems;
	if (m_orientation == orGrid)
	{
		int newline = (m_selected / m_max_columns);
		if (oldRow == newline)
			return oldTopLeft;
		
		int min = oldTopLeft * m_max_columns;
		int max = std::min(min + (m_max_rows * m_max_columns),m_content->size());

		if (m_selected >= min && m_selected < max)
			return oldTopLeft;

		int maxLines = ((m_content->size() + m_max_columns - 1) / m_max_columns) - m_max_rows;

		return std::min(std::max(oldTopLeft + (newline - oldRow),0), maxLines);
	}

	bool jumpBottom = (dir == moveBottom);

	if (dir == movePageDown && m_selected > max && !customPageSize)
	{
		jumpBottom = true;
	}

	if (doUp || (customPageSize && dir == movePageUp))
	{
		if (m_selected > oldSel)
		{
			jumpBottom = true;
		}
		else if (oldLine > 0)
			oldLine -= oldSel - m_selected;

		if (oldLine < 0 && m_selected > maxItems)
			oldLine = 0;
	}

	if (m_last_selectable_item == -1 && dir == justCheck)
	{
		m_content->cursorEnd();
		do
		{
			m_content->cursorMove(-1);
			m_last_selectable_item = m_content->cursorGet();
		} while (!m_content->currentCursorSelectable());
		m_content->cursorSet(m_selected);
	}

	if (doDown || dir == movePageDown)
	{

		int newline = oldLine + (m_selected - oldSel);
		if (newline < maxItems && newline > 0)
		{
			topLeft = oldSel - oldLine;
		}
		else
		{
			topLeft = m_selected - (maxItems - 1);
			if (m_selected < maxItems)
			{
				topLeft = 0;
			}
		}

		if (m_last_selectable_item != m_content->size() - 1 && m_selected >= m_last_selectable_item)
			jumpBottom = true;
	}

	if (jumpBottom)
	{
		topLeft = max;
	}
	else if (dir == justCheck)
	{
		if (m_first_selectable_item == -1)
		{
			m_first_selectable_item = 0;
			if (m_selected > 0)
			{
				m_content->cursorHome();
				if (!m_content->currentCursorSelectable())
				{
					do
					{
						m_content->cursorMove(1);
						m_first_selectable_item = m_content->cursorGet();
					} while (!m_content->currentCursorSelectable());
				}
				m_content->cursorSet(m_selected);
				if (oldLine == 0)
					oldLine = m_selected;
			}
		}
		if (indexChanged && m_selected < maxItems)
		{
			oldLine = m_selected;
		}

		// special case for initial draw
		topLeft = m_selected - oldLine;
		if (topLeft == 0 && m_selected > maxItems)
		{
			topLeft = m_selected - (maxItems / 2);
		}
	}
	else if (doUp || dir == movePageUp)
	{
		if (m_first_selectable_item > 0 && m_selected == m_first_selectable_item)
		{
			oldLine = m_selected;
		}
		topLeft = m_selected - oldLine;
	}

	if (topLeft < 0 || oldLine < 0)
	{
		topLeft = 0;
	}

	return topLeft;
}
void eListbox::setItemCornerRadiusInternal(uint8_t index, int radius, uint8_t edges)
{
	m_style.m_itemCornerRadius[index] = radius;
	m_style.m_itemCornerRadiusEdges[index] = edges;
}

void eListbox::setItemCornerRadius(int radius, uint8_t edges)
{
	for (uint8_t x = 0; x < 4; x++)
	{
		setItemCornerRadiusInternal(x, radius, edges);
	}
}

void eListbox::setItemCornerRadiusSelected(int radius, uint8_t edges)
{
	setItemCornerRadiusInternal(1, radius, edges);
}

void eListbox::setItemCornerRadiusMarked(int radius, uint8_t edges)
{
	setItemCornerRadiusInternal(2, radius, edges);
}

void eListbox::setItemCornerRadiusMarkedandSelected(int radius, uint8_t edges)
{
	setItemCornerRadiusInternal(3, radius, edges);
}

void eListbox::setItemGradientInternal(uint8_t index, const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	m_style.m_gradient_colors[index] = {startcolor, midcolor, endcolor};
	m_style.m_gradient_direction[index] = direction;
	m_style.m_gradient_alphablend[index] = alphablend;
	m_style.m_gradient_set[index] = true;
	invalidate();
}

void eListbox::setItemGradient(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	setItemGradientInternal(0, startcolor, midcolor, endcolor, direction, alphablend);
}

void eListbox::setItemGradientSelected(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	setItemGradientInternal(1, startcolor, midcolor, endcolor, direction, alphablend);
}

void eListbox::setItemGradientMarked(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	setItemGradientInternal(2, startcolor, midcolor, endcolor, direction, alphablend);
}

void eListbox::setItemGradientMarkedandSelected(const gRGB &startcolor, const gRGB &midcolor, const gRGB &endcolor, uint8_t direction, bool alphablend)
{
	setItemGradientInternal(3, startcolor, midcolor, endcolor, direction, alphablend);
}
