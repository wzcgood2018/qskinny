/******************************************************************************
 * QSkinny - Copyright (C) 2016 Uwe Rathmann
 * This file may be used under the terms of the QSkinny License, Version 1.0
 *****************************************************************************/

#include "QskControl.h"
#include "QskControlPrivate.h"

#include "QskAspect.h"
#include "QskFunctions.h"
#include "QskEvent.h"
#include "QskQuick.h"
#include "QskSetup.h"
#include "QskSkin.h"
#include "QskSkinlet.h"
#include "QskSkinHintTable.h"
#include "QskLayoutConstraint.h"

#include <qlocale.h>
#include <qvector.h>

QSK_SYSTEM_STATE( QskControl, Disabled, QskAspect::FirstSystemState )
QSK_SYSTEM_STATE( QskControl, Hovered, QskAspect::LastSystemState >> 1 )
QSK_SYSTEM_STATE( QskControl, Focused, QskAspect::LastSystemState )

static inline void qskSendEventTo( QObject* object, QEvent::Type type )
{
    QEvent event( type );
    QCoreApplication::sendEvent( object, &event );
}

QskControl::QskControl( QQuickItem* parent )
    : QskQuickItem( *( new QskControlPrivate() ), parent )
{
    Inherited::setActiveFocusOnTab( false );

    if ( parent )
    {
        // inheriting attributes from parent
        QskControlPrivate::resolveLocale( this );
    }
}

QskControl::~QskControl()
{
#if defined( QT_DEBUG )
    if ( auto w = window() )
    {
        // to catch suicide situations as a result of mouse clicks
        Q_ASSERT( this != w->mouseGrabberItem() );
    }
#endif
}

void QskControl::setAutoFillBackground( bool on )
{
    Q_D( QskControl );
    if ( on != d->autoFillBackground )
    {
        d->autoFillBackground = on;
        update();
    }
}

bool QskControl::autoFillBackground() const
{
    return d_func()->autoFillBackground;
}

void QskControl::setAutoLayoutChildren( bool on )
{
    Q_D( QskControl );
    if ( on != d->autoLayoutChildren )
    {
        d->autoLayoutChildren = on;
        if ( on )
            polish();
    }
}

bool QskControl::autoLayoutChildren() const
{
    return d_func()->autoLayoutChildren;
}

void QskControl::setWheelEnabled( bool on )
{
    Q_D( QskControl );
    if ( on != d->isWheelEnabled )
    {
        d->isWheelEnabled = on;
        Q_EMIT wheelEnabledChanged();
    }
}

bool QskControl::isWheelEnabled() const
{
    return d_func()->isWheelEnabled;
}

void QskControl::setFocusPolicy( Qt::FocusPolicy policy )
{
    Q_D( QskControl );
    if ( policy != d->focusPolicy )
    {
        d->focusPolicy = ( policy & ~Qt::TabFocus );
        Inherited::setActiveFocusOnTab( policy & Qt::TabFocus );

        Q_EMIT focusPolicyChanged();
    }
}

Qt::FocusPolicy QskControl::focusPolicy() const
{
    uint policy = d_func()->focusPolicy;
    if ( Inherited::activeFocusOnTab() )
        policy |= Qt::TabFocus;

    return static_cast< Qt::FocusPolicy >( policy );
}

void QskControl::setBackgroundColor( const QColor& color )
{
    setAutoFillBackground( true );
    setBackground( QskGradient( color ) );
}

void QskControl::setBackground( const QskGradient& gradient )
{
    using namespace QskAspect;
    const Aspect aspect = Control | Color;

    if ( hintTable().gradient( aspect ) != gradient )
    {
        setGradientHint( aspect, gradient );
        if ( autoFillBackground() )
            update();

        Q_EMIT backgroundChanged();
    }
}

void QskControl::resetBackground()
{
    using namespace QskAspect;
    const Aspect aspect = Control | Color;

    auto& table = hintTable();

    if ( table.hint( aspect ).isValid() )
    {
        table.removeHint( aspect );

        update();
        Q_EMIT backgroundChanged();
    }
}

QskGradient QskControl::background() const
{
    using namespace QskAspect;
    return gradientHint( Control );
}

void QskControl::setMargins( qreal margin )
{
    setMargins( QMarginsF( margin, margin, margin, margin ) );
}

void QskControl::setMargins( const QMarginsF& margins )
{
    using namespace QskAspect;

    const auto subControl = effectiveSubcontrol( QskAspect::Control );

    const QMarginsF m(
        qMax( qreal( margins.left() ), qreal( 0.0 ) ),
        qMax( qreal( margins.top() ), qreal( 0.0 ) ),
        qMax( qreal( margins.right() ), qreal( 0.0 ) ),
        qMax( qreal( margins.bottom() ), qreal( 0.0 ) ) );

    if ( m != this->margins() )
    {
        setMarginsHint( subControl | Margin, m );
        resetImplicitSize();

        Q_D( const QskControl );
        if ( polishOnResize() || d->autoLayoutChildren )
            polish();

        qskSendEventTo( this, QEvent::ContentsRectChange );
        Q_EMIT marginsChanged();
    }
}

void QskControl::resetMargins()
{
    using namespace QskAspect;
    const Aspect aspect = Control | Metric | Margin;

    const auto oldMargin = marginsHint( aspect );

    auto& table = hintTable();
    if ( table.hint( aspect ).isValid() )
    {
        table.removeHint( aspect );
        if ( marginsHint( aspect ) != oldMargin )
        {
            resetImplicitSize();

            Q_D( const QskControl );
            if ( polishOnResize() || d->autoLayoutChildren )
                polish();

            qskSendEventTo( this, QEvent::ContentsRectChange );
            Q_EMIT marginsChanged();
        }
    }
}

QMarginsF QskControl::margins() const
{
    return marginsHint( QskAspect::Control | QskAspect::Margin );
}

QRectF QskControl::contentsRect() const
{
    return qskValidOrEmptyInnerRect( rect(), margins() );
}

QRectF QskControl::subControlRect( QskAspect::Subcontrol subControl ) const
{
    return effectiveSkinlet()->subControlRect( this, contentsRect(), subControl );
}

QRectF QskControl::subControlRect(
    const QSizeF& size, QskAspect::Subcontrol subControl ) const
{
    QRectF rect( 0.0, 0.0, size.width(), size.height() );
    rect = qskValidOrEmptyInnerRect( rect, margins() );

    return effectiveSkinlet()->subControlRect( this, rect, subControl );
}

QLocale QskControl::locale() const
{
    return d_func()->locale;
}

void QskControl::setLocale( const QLocale& locale )
{
    Q_D( QskControl );

    d->explicitLocale = true;

    if ( d->locale != locale )
    {
        d->locale = locale;
        qskSendEventTo( this, QEvent::LocaleChange );
        qskSetup->inheritLocale( this, locale );
    }
}

void QskControl::resetLocale()
{
    Q_D( QskControl );

    if ( d->explicitLocale )
    {
        d->explicitLocale = false;
        QskControlPrivate::resolveLocale( this );
    }
}

void QskControl::initSizePolicy(
    QskSizePolicy::Policy horizontalPolicy,
    QskSizePolicy::Policy verticalPolicy )
{
    Q_D( QskControl );

    /*
       In constructors of derived classes you don't need
       to propagate changes by layoutConstraintChanged.
       Sometimes it is even worse as the parent might not be
       even prepared to handle the LayouRequest event.
     */

    d->sizePolicy.setHorizontalPolicy( horizontalPolicy );
    d->sizePolicy.setVerticalPolicy( verticalPolicy );

    if ( horizontalPolicy == QskSizePolicy::Constrained
        && verticalPolicy == QskSizePolicy::Constrained )
    {
        qWarning( "QskControl::initSizePolicy: conflicting constraints");
    }
}

void QskControl::setSizePolicy( QskSizePolicy policy )
{
    Q_D( QskControl );

    if ( policy != d->sizePolicy )
    {
        d->sizePolicy = policy;
        d->layoutConstraintChanged();

        if ( policy.policy( Qt::Horizontal ) == QskSizePolicy::Constrained
            && policy.policy( Qt::Vertical ) == QskSizePolicy::Constrained )
        {
            qWarning( "QskControl::setSizePolicy: conflicting constraints");
        }
    }
}

void QskControl::setSizePolicy(
    QskSizePolicy::Policy horizontalPolicy,
    QskSizePolicy::Policy verticalPolicy )
{
    setSizePolicy( QskSizePolicy( horizontalPolicy, verticalPolicy ) );
}

void QskControl::setSizePolicy(
    Qt::Orientation orientation, QskSizePolicy::Policy policy )
{
    Q_D( QskControl );

    if ( d->sizePolicy.policy( orientation ) != policy )
    {
        d->sizePolicy.setPolicy( orientation, policy );
        d->layoutConstraintChanged();
    }
}

QskSizePolicy QskControl::sizePolicy() const
{
    return d_func()->sizePolicy;
}

QskSizePolicy::Policy QskControl::sizePolicy( Qt::Orientation orientation ) const
{
    return d_func()->sizePolicy.policy( orientation );
}

/*
    Layout attributes belong more to the layout code, than
    being parameters of the control. So storing them here is kind of a
    design flaw ( similar to QWidget/QSizePolicy ).
    But this way we don't need to add the attributes to all type of
    layout engines + we can make use of them when doing layouts
    manually ( f.e autoLayoutChildren ).
 */
void QskControl::setLayoutAlignmentHint( Qt::Alignment alignment )
{
    Q_D( QskControl );

    if ( d->layoutAlignmentHint != alignment )
    {
        d->layoutAlignmentHint = alignment;
        d->layoutConstraintChanged();
    }
}

Qt::Alignment QskControl::layoutAlignmentHint() const
{
    return static_cast< Qt::Alignment >( d_func()->layoutAlignmentHint );
}

void QskControl::setLayoutHint( LayoutHint flag, bool on )
{
    Q_D( QskControl );
    if ( ( d->layoutHints & flag ) != on )
    {
        if ( on )
            d->layoutHints |= flag;
        else
            d->layoutHints &= ~flag;
    
        d->layoutConstraintChanged();
    }
}

bool QskControl::testLayoutHint( LayoutHint hint ) const
{
    return d_func()->layoutHints & hint;
}

void QskControl::setLayoutHints( LayoutHints hints )
{
    Q_D( QskControl );
    if ( hints != layoutHints() )
    {
        d->layoutHints = hints;
        d->layoutConstraintChanged();
    }
}

QskControl::LayoutHints QskControl::layoutHints() const
{
    return static_cast< LayoutHints >( d_func()->layoutHints );
}

void QskControl::setPreferredSize( const QSizeF& size )
{
    setExplicitSizeHint( Qt::PreferredSize, size );
}

void QskControl::setPreferredSize( qreal width, qreal height )
{
    setPreferredSize( QSizeF( width, height ) );
}

void QskControl::setPreferredWidth( qreal width )
{
    setPreferredSize( QSizeF( width, preferredSize().height() ) );
}

void QskControl::setPreferredHeight( qreal height )
{
    setPreferredSize( QSizeF( preferredSize().width(), height ) );
}

void QskControl::setMinimumSize( const QSizeF& size )
{
    setExplicitSizeHint( Qt::MinimumSize, size );
}

void QskControl::setMinimumSize( qreal width, qreal height )
{
    setMinimumSize( QSizeF( width, height ) );
}

void QskControl::setMinimumWidth( qreal width )
{
    setMinimumSize( QSizeF( width, minimumSize().height() ) );
}

void QskControl::setMinimumHeight( qreal height )
{
    setMinimumSize( QSizeF( minimumSize().width(), height ) );
}

void QskControl::setMaximumSize( const QSizeF& size )
{
    setExplicitSizeHint( Qt::MaximumSize, size );
}

void QskControl::setMaximumSize( qreal width, qreal height )
{
    setMaximumSize( QSizeF( width, height ) );
}

void QskControl::setMaximumWidth( qreal width )
{
    setMaximumSize( QSizeF( width, maximumSize().height() ) );
}

void QskControl::setMaximumHeight( qreal height )
{
    setMaximumSize( QSizeF( maximumSize().width(), height ) );
}

void QskControl::setFixedSize( const QSizeF& size )
{
    const QSizeF newSize = size.expandedTo( QSizeF( 0, 0 ) );

    const QskSizePolicy policy( QskSizePolicy::Fixed, QskSizePolicy::Fixed );

    Q_D( QskControl );

    if ( policy != d->sizePolicy ||
        d->explicitSizeHint( Qt::PreferredSize ) != newSize )
    {
        d->sizePolicy = policy;
        d->setExplicitSizeHint( Qt::PreferredSize, newSize );

        d->layoutConstraintChanged();
    }
}

void QskControl::setFixedSize( qreal width, qreal height )
{
    setFixedSize( QSizeF( width, height ) );
}

void QskControl::setFixedWidth( qreal width )
{
    if ( width < 0 )
        width = 0;

    Q_D( QskControl );

    auto size = d->explicitSizeHint( Qt::PreferredSize );

    if ( ( d->sizePolicy.horizontalPolicy() != QskSizePolicy::Fixed ) ||
        ( size.width() != width ) )
    {
        size.setWidth( width );

        d->sizePolicy.setHorizontalPolicy( QskSizePolicy::Fixed );
        d->setExplicitSizeHint( Qt::PreferredSize, size );

        d->layoutConstraintChanged();
    }
}

void QskControl::setFixedHeight( qreal height )
{
    if ( height < 0 )
        height = 0;

    Q_D( QskControl );

    auto size = d->explicitSizeHint( Qt::PreferredSize );

    if ( ( d->sizePolicy.verticalPolicy() != QskSizePolicy::Fixed ) ||
        ( size.height() != height ) )
    {
        size.setHeight( height );

        d->sizePolicy.setVerticalPolicy( QskSizePolicy::Fixed );
        d->setExplicitSizeHint( Qt::PreferredSize, size );

        d->layoutConstraintChanged();
    }
}

void QskControl::resetExplicitSizeHint( Qt::SizeHint whichHint )
{
    if ( whichHint >= Qt::MinimumSize && whichHint <= Qt::MaximumSize )
    {
        Q_D( QskControl );

        const auto oldHint = d->explicitSizeHint( whichHint );
        d->resetExplicitSizeHint( whichHint );

        if ( oldHint != d->explicitSizeHint( whichHint ) )
            d->layoutConstraintChanged();
    }
}

void QskControl::setExplicitSizeHint( Qt::SizeHint whichHint, const QSizeF& size )
{
    if ( whichHint >= Qt::MinimumSize && whichHint <= Qt::MaximumSize )
    {
        const QSizeF newSize( ( size.width() < 0 ) ? -1.0 : size.width(),
            ( size.height() < 0 ) ? -1.0 : size.height() );

        Q_D( QskControl );

        if ( newSize != d->explicitSizeHint( whichHint ) )
        {
            d->setExplicitSizeHint( whichHint, newSize );
            d->layoutConstraintChanged();
        }
    }
}

void QskControl::setExplicitSizeHint(
    Qt::SizeHint whichHint, qreal width, qreal height )
{
    setExplicitSizeHint( whichHint, QSizeF( width, height ) );
}

QSizeF QskControl::explicitSizeHint( Qt::SizeHint whichHint ) const
{
    if ( whichHint >= Qt::MinimumSize && whichHint <= Qt::MaximumSize )
        return d_func()->explicitSizeHint( whichHint );

    return QSizeF( -1, -1 );
}

QSizeF QskControl::implicitSizeHint(
    Qt::SizeHint whichHint, const QSizeF& constraint ) const
{
    if ( whichHint < Qt::MinimumSize || whichHint > Qt::MaximumSize )
        return QSizeF( -1, -1 );

    if ( constraint.isValid() )
    {
        // having constraints in both directions does not make sense
        return constraint;
    }

    QSizeF hint( -1, -1 );

    if ( whichHint == Qt::PreferredSize )
    {
        if ( constraint.width() >= 0 )
        {
            hint.setWidth( constraint.width() );
            hint.setHeight( heightForWidth( constraint.width() ) );
        }
        else if ( constraint.height() >= 0 )
        {
            hint.setWidth( widthForHeight( constraint.height() ) );
            hint.setHeight( constraint.height() );
        }
        else
        {
            hint = implicitSize();
        }
    }
    else
    {
        // TODO ...
    }

    return hint;
}

QSizeF QskControl::effectiveSizeHint(
    Qt::SizeHint whichHint, const QSizeF& constraint ) const
{
    if ( whichHint < Qt::MinimumSize || whichHint > Qt::MaximumSize )
        return QSizeF( 0, 0 );

    Q_D( const QskControl );

    d->blockLayoutRequestEvents = false;

    /*
        The explicit size has always precedence over te implicit size.

        Implicit sizes are currently only implemented for preferred and
        explicitSizeHint returns valid explicit hints for minimum/maximum
        even if not being set by application code.
     */

    QSizeF hint = d->explicitSizeHint( whichHint );

    if ( !hint.isValid() )
    {
#if 0
        if ( hint.width() >= 0 && constraint.width() >= 0 )
            constraint.setWidth( hint.width() );

        if ( hint.height() >= 0 && constraint.height() >= 0 )
            constraint.setHeight( hint.height() );
#endif

        const auto implicit = implicitSizeHint( whichHint, constraint );

        if ( hint.width() < 0 )
            hint.setWidth( implicit.width() );

        if ( hint.height() < 0 )
            hint.setHeight( implicit.height() );
    }

    if ( hint.width() >= 0 || hint.height() >= 0 )
    {
        /*
            We might need to normalize the hints, so that
            we always have: minimum <= preferred <= maximum.
         */

#define DEBUG_NORMALIZE_HINTS 0

#if DEBUG_NORMALIZE_HINTS
        const auto oldHint = hint;
#endif

        if ( whichHint == Qt::MaximumSize )
        {
            const auto minimumHint = d->explicitSizeHint( Qt::MinimumSize );

            if ( hint.width() >= 0 )
                hint.setWidth( qMax( hint.width(), minimumHint.width() ) );

            if ( hint.height() >= 0 )
                hint.setHeight( qMax( hint.height(), minimumHint.height() ) );
        }
        else if ( whichHint == Qt::PreferredSize )
        {
            const auto minimumHint = d->explicitSizeHint( Qt::MinimumSize );
            const auto maximumHint = d->explicitSizeHint( Qt::MaximumSize );

            if ( hint.width() >= 0 )
            {
                const auto minW = minimumHint.width();
                const auto maxW = qMax( minW, maximumHint.width() );

                hint.setWidth( qBound( minW, hint.width(), maxW ) );
            }

            if ( hint.height() >= 0 )
            {
                const auto minH = minimumHint.height();
                const auto maxH = qMax( minH, maximumHint.height() );

                hint.setHeight( qBound( minH, hint.height(), maxH ) );
            }
        }

#if DEBUG_NORMALIZE_HINTS
        if ( hint != oldHint )
        {
            const auto minimumHint = d->explicitSizeHint( Qt::MinimumSize );
            const auto maximumHint = d->explicitSizeHint( Qt::MaximumSize );
            qDebug() << whichHint << minimumHint << oldHint << maximumHint << "->" << hint;
        }
#endif
    }

    return hint;
}

qreal QskControl::heightForWidth( qreal width ) const
{
    Q_D( const QskControl );

    d->blockLayoutRequestEvents = false;

    if ( !d->autoLayoutChildren )
        return -1.0;

    using namespace QskLayoutConstraint;

    return constrainedMetric(
        HeightForWidth, this, width, constrainedChildrenMetric );
}

qreal QskControl::widthForHeight( qreal height ) const
{
    Q_D( const QskControl );

    d->blockLayoutRequestEvents = false;

    if ( !d->autoLayoutChildren )
        return -1.0;

    using namespace QskLayoutConstraint;

    return constrainedMetric(
        WidthForHeight, this, height, constrainedChildrenMetric );
}

bool QskControl::event( QEvent* event )
{
    switch ( static_cast< int >( event->type() ) )
    {
        case QEvent::EnabledChange:
        {
            setSkinStateFlag( Disabled, !isEnabled() );
            break;
        }
        case QEvent::LocaleChange:
        {
            Q_EMIT localeChanged( locale() );
            break;
        }
        case QEvent::LayoutRequest:
        {
            if ( d_func()->autoLayoutChildren )
                resetImplicitSize();

            break;
        }
        case QEvent::StyleChange:
        {
            // The skin has changed

            if ( skinlet() == nullptr )
            {
                /*
                    When we don't have a local skinlet, the skinlet
                    from the previous skin might be cached.
                 */

                setSkinlet( nullptr );
            }

            break;
        }
        case QskEvent::Gesture:
        {
            gestureEvent( static_cast< QskGestureEvent* >( event ) );
            return true;
        }
    }

    if ( d_func()->maybeGesture( this, event ) )
        return true;

    return Inherited::event( event );
}

bool QskControl::childMouseEventFilter( QQuickItem* item, QEvent* event )
{
#if QT_VERSION < QT_VERSION_CHECK( 5, 10, 0 )

    if ( event->type() == QEvent::MouseButtonPress )
    {
        auto me = static_cast< QMouseEvent* >( event );

        if ( me->source() == Qt::MouseEventSynthesizedByQt )
        {
            /*
                Unhandled touch events result in creating synthetic
                mouse events. For all versions < 5.10 those events are
                passed through childMouseEventFilter without doing the
                extra checks, that are done for real mouse events.
                Furthermore the coordinates are relative
                to this - not to item.

                To avoid having a different behavior between using
                mouse and touch, we do those checks here.
             */

            auto itm = item;
            auto pos = item->mapFromScene( me->windowPos() );

            for ( itm = item; itm != this; itm = itm->parentItem() )
            {
                if ( itm->acceptedMouseButtons() & me->button() )
                {
                    if ( itm->contains( pos ) )
                        break;
                }

                pos += QPointF( itm->x(), itm->y() );
            }

            if ( itm != item )
            {
                if ( itm == this )
                    return false;

                QScopedPointer< QMouseEvent > clonedEvent(
                    QQuickWindowPrivate::cloneMouseEvent( me, &pos ) );

                return d_func()->maybeGesture( itm, clonedEvent.data() );
            }
        }
    }

#endif

    return d_func()->maybeGesture( item, event );
}

bool QskControl::gestureFilter( QQuickItem*, QEvent* )
{
    return false;
}

void QskControl::gestureEvent( QskGestureEvent* )
{
}

void QskControl::hoverEnterEvent( QHoverEvent* event )
{
    Inherited::hoverEnterEvent( event );
    setSkinStateFlag( Hovered );
}

void QskControl::hoverLeaveEvent( QHoverEvent* event )
{
    Inherited::hoverLeaveEvent( event );
    setSkinStateFlag( Hovered, false );
}

void QskControl::itemChange( QQuickItem::ItemChange change,
    const QQuickItem::ItemChangeData& value )
{
    switch ( static_cast< int >( change ) )
    {
        case QQuickItem::ItemParentHasChanged:
        {
            if ( value.item )
            {
                if ( !d_func()->explicitLocale )
                    QskControlPrivate::resolveLocale( this );
            }

#if 1
            // not necessarily correct, when parent != parentItem ???
            qskSendEventTo( this, QEvent::ParentChange );
#endif

            break;
        }
        case QQuickItem::ItemChildAddedChange:
        {
            if ( autoLayoutChildren() && !qskIsTransparentForPositioner( value.item ) )
                polish();

            break;
        }
        case QQuickItem::ItemActiveFocusHasChanged:
        {
            setSkinStateFlag( Focused, hasActiveFocus() );
            break;
        }
    }

    Inherited::itemChange( change, value );
}

void QskControl::geometryChanged(
    const QRectF& newGeometry, const QRectF& oldGeometry )
{
    if ( d_func()->autoLayoutChildren )
    {
        if ( newGeometry.size() != oldGeometry.size() )
            polish();
    }

    Inherited::geometryChanged( newGeometry, oldGeometry );
}

void QskControl::windowDeactivateEvent()
{
    // stopping gesture recognition ???
    Inherited::windowDeactivateEvent();
}

void QskControl::updateItemPolish()
{
    if ( d_func()->autoLayoutChildren && !maybeUnresized() )
    {
        const auto rect = layoutRect();

        const auto children = childItems();
        for ( auto child : children )
        {
            if ( !qskIsTransparentForPositioner( child ) )
            {
                // rect = QskLayoutConstraint::itemRect( info.item, rect, ... );
                qskSetItemGeometry( child, rect );
            }
        }
    }

    updateResources();
    updateLayout();
}

QSGNode* QskControl::updateItemPaintNode( QSGNode* node )
{
    if ( node == nullptr )
        node = new QSGNode;

    updateNode( node );
    return node;
}

QskControl* QskControl::owningControl() const
{
    return const_cast< QskControl* >( this );
}

QRectF QskControl::layoutRect() const
{
    Q_D( const QskControl );

    if ( d->width <= 0.0 && d->height <= 0.0 )
        return QRectF();

    return layoutRectForSize( size() );
}

QRectF QskControl::layoutRectForSize( const QSizeF& size ) const
{
    const QRectF r( 0.0, 0.0, size.width(), size.height() );
    return qskValidOrEmptyInnerRect( r, margins() );
}

QRectF QskControl::gestureRect() const
{
    return rect();
}

QRectF QskControl::focusIndicatorRect() const
{
    return contentsRect();
}

void QskControl::updateLayout()
{
}

void QskControl::updateResources()
{
}

QSizeF QskControl::contentsSizeHint() const
{
    qreal w = -1; // no hint
    qreal h = -1;

    if ( d_func()->autoLayoutChildren )
    {
        const auto children = childItems();
        for ( const auto child : children )
        {
            if ( auto control = qskControlCast( child ) )
            {
                if ( !control->isTransparentForPositioner() )
                {
                    const QSizeF hint = control->sizeHint();

                    w = qMax( w, hint.width() );
                    h = qMax( h, hint.height() );
                }
            }
        }
    }

    return QSizeF( w, h );
}

QVector< QskAspect::Subcontrol > QskControl::subControls() const
{
    using namespace QskAspect;

    QVector< Subcontrol > subControls;

    for ( auto mo = metaObject(); mo != nullptr; mo = mo->superClass() )
    {
        const auto moSubControls = QskAspect::subControls( mo );

        for ( auto subControl : moSubControls )
        {
            const auto subControlEffective = effectiveSubcontrol( subControl );

            if ( subControlEffective == subControl )
            {
                subControls += subControlEffective;
            }
            else
            {
                // when subControlEffective differs it usually has
                // been mapped to a subcontrol of an inherited class
                // that is already in the list.

                if ( !subControls.contains( subControlEffective ) )
                {
                    subControls += subControlEffective;
                }
            }
        }
    }

    return subControls;
}

#include "moc_QskControl.cpp"
