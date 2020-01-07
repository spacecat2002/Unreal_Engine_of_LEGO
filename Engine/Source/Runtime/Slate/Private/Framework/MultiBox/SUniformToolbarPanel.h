// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Layout/Visibility.h"
#include "SlotBase.h"
#include "Layout/Margin.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"

class FArrangedChildren;


/**
 * This panel evenly divides up the available space between all of its children but allows for a min and max threshold to be supplied so that
 * very large children do not force siblings to grow unnecessarily large and very small children are not forced to be unnecessarily large to maintain uniformity
 */
class SLATE_API SUniformToolbarPanel : public SPanel
{
public:
	/** Stores the per-child info for this panel type */
	struct FSlot : public TSlotBase<FSlot>, public TSupportsContentAlignmentMixin<FSlot>
	{
		FSlot()
		: TSlotBase<FSlot>()
		, TSupportsContentAlignmentMixin<FSlot>(HAlign_Fill, VAlign_Fill)
		{
		}

	};

	SUniformToolbarPanel();

	/**
	 * Used by declarative syntax to create a Slot in the specified Column, Row.
	 */
	static FSlot& Slot()
	{
		return *(new FSlot());
	}

	SLATE_BEGIN_ARGS(SUniformToolbarPanel)
		: _Orientation(EOrientation::Orient_Horizontal)
		, _SlotPadding(FMargin(0.0f))
		, _MinDesiredSlotSize(FVector2D(0.0f,0.0f))
		, _MaxUniformSize(0.0f)
		, _MinUniformSize(0.0f)
		{
			_Visibility = EVisibility::SelfHitTestInvisible;
		}

		/** Slot type supported by this panel */
		SLATE_SUPPORTS_SLOT(FSlot)
		 
		/** Orientation (i.e major axis for this panel) */
		SLATE_ARGUMENT(EOrientation, Orientation)

		/** Padding given to each slot */
		SLATE_ATTRIBUTE(FMargin, SlotPadding)

		/** The minimum desired width and height of the slots */
		SLATE_ATTRIBUTE(FVector2D, MinDesiredSlotSize)

		/** 
		 * The maximum size a child can be to be uniformly arranged.  Widgets over this size will not be uniformly sized and will not contribute to the uniform size of siblings. 
		 * If this value is 0, all widgets will be uniformly sized
		 */
		SLATE_ATTRIBUTE(float, MaxUniformSize)

		/**
		 * The minimum size a child must be to be uniformly arranged.  Widgets under this size will not be uniformly sized. 
		 * If this value is 0 all widgets will be uniformly sized
		 */
		SLATE_ATTRIBUTE(float, MinUniformSize)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	//~ Begin SPanel Interface	
	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FChildren* GetChildren() override;
	//~ End SPanel Interface

	/** See SlotPadding attribute */
	void SetSlotPadding(TAttribute<FMargin> InSlotPadding);

	/**
	 * Dynamically add a new slot to the Panel
	 *
	 * @return A reference to the newly-added slot
	 */
	FSlot& AddSlot();
	
	/**
	 * Removes a slot from this panel which contains the specified SWidget
	 *
	 * @param SlotWidget The widget to match when searching through the slots
	 * @returns The true if the slot was removed and false if no slot was found matching the widget
	 */
	bool RemoveSlot( const TSharedRef<SWidget>& SlotWidget );

	/** Removes all slots from the panel */
	void ClearChildren();
protected:
	// Begin SWidget overrides.
	virtual FVector2D ComputeDesiredSize(float) const override;
	// End SWidget overrides.

private:
	TPanelChildren<FSlot> Children;
	TAttribute<FMargin> SlotPadding;
	
	mutable float MajorAxisUniformDesiredSize = 0;

	EOrientation Orientation;
	TAttribute<FVector2D> MinDesiredSlotSize;
	TAttribute<float> MaxUniformSize;
	TAttribute<float> MinUniformSize;
};
