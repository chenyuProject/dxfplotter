#include <viewport.h>
#include <pathitem.h>

#include <QDebug> // TODO

namespace View
{

constexpr int rubberBandTolerance = 2;
// Rectangle extend used when clicking with empty rubber band.
constexpr QPoint pointSelectionRectExtend(10, 10);

void Viewport::setupPathItems()
{
	m_task->forEachPath(
		[scene = scene()](Model::Path *path) {
			PathItem *item = new PathItem(path);
			scene->addItem(item);
		}
	);
}

void Viewport::startMovement(const QPoint &mousePos)
{
	m_lastMousePosition = mousePos;
}

void Viewport::updateMovement(const QPoint &mousePos)
{
	const QPointF delta = mapToScene(mousePos) - mapToScene(m_lastMousePosition);

	// Disable anchor to avoid interferences
	setTransformationAnchor(NoAnchor);

	translate(delta.x(), delta.y());

	// Restore anchor
	setTransformationAnchor(AnchorUnderMouse);

	m_lastMousePosition = mousePos;
}

void Viewport::startRubberBand(const QPoint &mousePos)
{
	m_rubberBand.start(mousePos, mapToScene(mousePos));
}

void Viewport::updateRubberBand(const QPoint &mousePos)
{
	m_rubberBand.update(mousePos, mapToScene(mousePos));
}

void Viewport::endRubberBand(const QPoint &mousePos, bool addToSelection)
{
	m_rubberBand.end(mousePos, mapToScene(mousePos));

	// Point selection
	if (m_rubberBand.empty(rubberBandTolerance)) {
		// Fake selection area
		const QRect rect(mousePos - pointSelectionRectExtend, mousePos + pointSelectionRectExtend);

		// Find items in fake selection area
		const QList<QGraphicsItem *> items = QGraphicsView::items(rect);
		if (!items.empty()) {
			// Obtain only the first item.
			QGraphicsItem *item = items.front();

			if (!addToSelection) {
				// Clear all and select one in replacive selection
				scene()->clearSelection();
				item->setSelected(true);
			}
			else {
				// Toggle selection in additive selection
				item->setSelected(!item->isSelected());
			}
		}
	}
	// Area selection
	else {
		// Create a path with selection area
		QPainterPath path;
		path.addRect(m_rubberBand.rect());

		scene()->setSelectionArea(path, addToSelection ? Qt::AddToSelection : Qt::ReplaceSelection);
	}
}

void Viewport::setupModel()
{
	setScene(new QGraphicsScene());

	scene()->addItem(&m_rubberBand);

	setupPathItems();

	// Expand scene rect by margin allowing moving out of bound
	static const float rectMargin = 200.0f;
	const QRectF rect = sceneRect() + QMarginsF(rectMargin, rectMargin, rectMargin, rectMargin);
	setSceneRect(rect);
}

/** @brief Painter for grid and axis into background
 */
class BackgroundPainter
{
private:
	QPainter *m_painter;
	const QRectF &m_sceneRect;
	float m_pixelRatio;

	// Draw grid using dashed lines
	void drawLineGrid(float step)
	{
		// Point grid color
		static const QBrush brush(QColor(120, 120, 120));
		static const QPen pen(brush, 0, Qt::DashLine);

		m_painter->setPen(pen);

		const float startX = m_sceneRect.left();
		const float startY = m_sceneRect.top();
		const float endX = m_sceneRect.right();
		const float endY = m_sceneRect.bottom();

		// Reduce resolution to have the same as the grid
		const float gridStartX = std::ceil(startX / step) * step;
		const float gridStartY = std::ceil(startY / step) * step;

		for (float x = gridStartX; x <= endX; x += step) {
			const QPointF start(x, startY);
			const QPointF end(x, endY);
			m_painter->drawLine(start, end);
		}

		for (float y = gridStartY; y <= endY; y += step) {
			const QPointF start(startX, y);
			const QPointF end(endX, y);
			m_painter->drawLine(start, end);
		}
	}

	// Draw grid using points
	void drawPointGrid(float step)
	{
		// Point grid color
		static const QBrush brush(QColor(180, 180, 180));
		static const QPen pen(brush, 0);

		m_painter->setPen(pen);

		// Reduce resolution to have the same as the grid
		const float gridStartX = std::ceil(m_sceneRect.left() / step) * step;
		const float gridStartY = std::ceil(m_sceneRect.top() / step) * step;

		for (float x = gridStartX, endX = m_sceneRect.right(); x <= endX; x += step) {
			for (float y = gridStartY, endY = m_sceneRect.bottom(); y <= endY; y += step) {
				const QPointF point(x, y);
				m_painter->drawPoint(point);
			}
		}
	}

	void drawGrid()
	{
		// Minimum pixel spacing two lines
		static const float minimumPixelStep = 20;

		const int resolution = std::ceil(std::log10(m_pixelRatio * minimumPixelStep));
		const float step = std::pow(10.0f, resolution);

		drawPointGrid(step);
		drawLineGrid(step * 10.0f);
	}

	void drawOriginAxis(const QPointF &dir, float scale, const QPen &pen)
	{
		static const QPointF center(0.0f, 0.0f);

		m_painter->setPen(pen);
		m_painter->drawLine(center, center + dir * scale);
	}

	void drawOrigin()
	{
		// X axis color
		static const QBrush xBrush(QColor(255, 0, 0));
		static const QPen xPen(xBrush, 0);

		// Y axis color
		static const QBrush yBrush(QColor(0, 255, 0));
		static const QPen yPen(yBrush, 0);

		static const QPointF x(1.0f, 0.0f);
		static const QPointF y(0.0f, 1.0f);

		// Axis size in pixel
		static const int axisSize = 20;

		const float scale = axisSize * m_pixelRatio;

		// raw each axis
		drawOriginAxis(x, scale, xPen);
		drawOriginAxis(y, scale, yPen);
	}

public:
	explicit BackgroundPainter(QPainter *painter, const QRectF &sceneRect, float pixelRatio)
		:m_painter(painter),
		m_sceneRect(sceneRect),
		m_pixelRatio(pixelRatio)
	{
		drawGrid();
		drawOrigin();
	}
};

void Viewport::taskChanged()
{
	setupModel();
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	constexpr float SCALE_STEP = 0.2f;

	const float factor = 1.0f + ((event->delta() > 0) ? SCALE_STEP : -SCALE_STEP);

	scale(factor, factor);

	event->accept();
}

void Viewport::mousePressEvent(QMouseEvent *event)
{
	const QPoint &mousePos = event->pos();

	switch (event->button()) {
		case Qt::MidButton:
		{
			startMovement(mousePos);
			break;
		}
		case Qt::LeftButton:
		{
			startRubberBand(mousePos);
			break;
		}
		default:
		{
			break;
		}
	}

	event->accept();
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)
{
	const QPoint &mousePos = event->pos();

	switch (event->button()) {
		case Qt::LeftButton:
		{
			const bool addToSelection = event->modifiers() & Qt::ControlModifier;
			endRubberBand(mousePos, addToSelection);
			break;
		}
		default:
		{
			break;
		}
	}

	event->accept();
}

void Viewport::mouseMoveEvent(QMouseEvent *event)
{
	const QPoint &mousePos = event->pos();
	const Qt::MouseButtons buttons = event->buttons();

	if (buttons & Qt::MiddleButton) {
		updateMovement(mousePos);
	}
	if (buttons & Qt::LeftButton) {
		updateRubberBand(mousePos);
	}

	// Forward event used for anchors
	QGraphicsView::mouseMoveEvent(event);

	emit cursorMoved(mapToScene(mousePos));
}

void Viewport::drawBackground(QPainter *painter, const QRectF &updatedRect)
{

	const QRect screenRect = rect();
	const QRectF sceneRect = mapToScene(rect()).boundingRect();

	const float pixelRatio = std::max(sceneRect.width() / screenRect.width(), sceneRect.height() / screenRect.height());

	// Basic background color
	static const QBrush brush(QColor(0, 0, 0));
	painter->fillRect(updatedRect, brush);

	BackgroundPainter backgroundPainter(painter, sceneRect, pixelRatio);
}

Viewport::Viewport(Model::Application &app)
	:TaskModelObserver(app)
{
	// Setup default empty scene
	setScene(new QGraphicsScene());

	// Invert Y axis
	scale(1.0f, -1.0f);

	// Disable dragging support
	setDragMode(NoDrag);

	// Anchor under mouse for zooming
	setResizeAnchor(NoAnchor);
	setTransformationAnchor(AnchorUnderMouse);

	setRenderHints(QPainter::Antialiasing);

	// Hide scroll bars
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

}
