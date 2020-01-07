// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SUniformToolbarPanel.h"
#include "Layout/LayoutUtils.h"

SUniformToolbarPanel::SUniformToolbarPanel()
: Children(this)
{
}

void SUniformToolbarPanel::Construct( const FArguments& InArgs )
{
	SlotPadding = InArgs._SlotPadding;

	MinDesiredSlotSize = InArgs._MinDesiredSlotSize;
	MaxUniformSize = InArgs._MaxUniformSize;
	MinUniformSize = InArgs._MinUniformSize;

	Orientation = InArgs._Orientation;

	Children.Reserve( InArgs.Slots.Num() );
	for (int32 ChildIndex=0; ChildIndex < InArgs.Slots.Num(); ChildIndex++)
	{
		FSlot* ChildSlot = InArgs.Slots[ChildIndex];
		Children.Add( ChildSlot );
	}
}

void SUniformToolbarPanel::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	if ( Children.Num() > 0 )
	{
		const float MaxUniformSizeVal = MaxUniformSize.Get();
		const float MinUniformSizeVal = MinUniformSize.Get();

		const FVector2D UniformCellSize = Orientation == EOrientation::Orient_Horizontal
			? FVector2D(FMath::Min(AllottedGeometry.GetLocalSize().X/Children.Num(), MajorAxisUniformDesiredSize), AllottedGeometry.GetLocalSize().Y)
			: FVector2D(AllottedGeometry.GetLocalSize().X, FMath::Min(AllottedGeometry.GetLocalSize().Y / Children.Num(), MajorAxisUniformDesiredSize));

		const FVector2D SlotPaddingDesiredSize = SlotPadding.Get().GetDesiredSize();

		const FMargin& CurrentSlotPadding(SlotPadding.Get());
		FVector2D CurrentOffset = FVector2D::ZeroVector;


		for ( int32 ChildIndex=0; ChildIndex < Children.Num(); ++ChildIndex )
		{
			const FSlot& Child = Children[ChildIndex];
			const EVisibility ChildVisibility = Child.GetWidget()->GetVisibility();
			if ( ArrangedChildren.Accepts(ChildVisibility) )
			{
				AlignmentArrangeResult XAxisResult(0,0); 
				AlignmentArrangeResult YAxisResult(0,0);

				const FVector2D ChildDesiredSize = Child.GetWidget()->GetDesiredSize() + SlotPaddingDesiredSize;
				const float MajorAxisDesiredSize = Orientation == EOrientation::Orient_Horizontal ? ChildDesiredSize.X : ChildDesiredSize.Y;

				if ((MaxUniformSizeVal > 0.0f && MajorAxisDesiredSize > MaxUniformSizeVal)
					|| (MinUniformSizeVal > 0.0f && MajorAxisDesiredSize < MinUniformSizeVal))
				{
					// Do the standard arrangement of elements within a slot
					// Takes care of alignment and padding.
					XAxisResult = AlignChild<Orient_Horizontal>(ChildDesiredSize.X, Child, CurrentSlotPadding);
					YAxisResult = AlignChild<Orient_Vertical>(UniformCellSize.Y, Child, CurrentSlotPadding);
				}
				else
				{
					// Do the standard arrangement of elements within a slot
					// Takes care of alignment and padding.
					XAxisResult = AlignChild<Orient_Horizontal>(UniformCellSize.X, Child, CurrentSlotPadding);
					YAxisResult = AlignChild<Orient_Vertical>(UniformCellSize.Y, Child, CurrentSlotPadding);
				}

				if(Orientation == EOrientation::Orient_Horizontal)
				{
					ArrangedChildren.AddWidget(ChildVisibility,
						AllottedGeometry.MakeChild(Child.GetWidget(),
							FVector2D(CurrentOffset.X + XAxisResult.Offset, YAxisResult.Offset),
							FVector2D(XAxisResult.Size, YAxisResult.Size)
						));
				}
				else
				{
					ArrangedChildren.AddWidget(ChildVisibility,
						AllottedGeometry.MakeChild(Child.GetWidget(),
							FVector2D(XAxisResult.Offset, CurrentOffset.Y + YAxisResult.Offset),
							FVector2D(XAxisResult.Size, YAxisResult.Size)
						));
				}
				CurrentOffset += FVector2D(XAxisResult.Size, YAxisResult.Size);
			}

		}
	}
}

FVector2D SUniformToolbarPanel::ComputeDesiredSize( float ) const
{
	FVector2D MaxUniformChildDesiredSize = FVector2D::ZeroVector;
	FVector2D NonUniformChildDesiredSize = FVector2D::ZeroVector;
	const FVector2D SlotPaddingDesiredSize = SlotPadding.Get().GetDesiredSize();
	
	const FVector2D CachedMinDesiredSlotSize = MinDesiredSlotSize.Get();

	const float MaxUniformSizeVal = MaxUniformSize.Get();
	const float MinUniformSizeVal = MinUniformSize.Get();

	int32 NumUniformCells = 0;

	float MinorAxisUniformDesiredSize = 0;

	const int32 NumChildren = Children.Num();
	for ( int32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex )
	{
		const FSlot& Child = Children[ ChildIndex ];

		if (Child.GetWidget()->GetVisibility() != EVisibility::Collapsed)
		{	
			FVector2D ChildDesiredSize = Child.GetWidget()->GetDesiredSize() + SlotPaddingDesiredSize;

			ChildDesiredSize.X = FMath::Max(ChildDesiredSize.X, CachedMinDesiredSlotSize.X);
			ChildDesiredSize.Y = FMath::Max(ChildDesiredSize.Y, CachedMinDesiredSlotSize.Y);

			const float MajorAxisDesiredSize = Orientation == EOrientation::Orient_Horizontal ? ChildDesiredSize.X : ChildDesiredSize.Y;
			const float MinorAxisDesiredSize = Orientation == EOrientation::Orient_Horizontal ? ChildDesiredSize.Y : ChildDesiredSize.X;

			if ((MaxUniformSizeVal > 0.0f && MajorAxisDesiredSize > MaxUniformSizeVal)
				|| (MinUniformSizeVal > 0.0f && MajorAxisDesiredSize < MinUniformSizeVal))
			{
				NonUniformChildDesiredSize += ChildDesiredSize;
			}
			else
			{
				++NumUniformCells;
				MajorAxisUniformDesiredSize = FMath::Max(MajorAxisUniformDesiredSize, MajorAxisDesiredSize);
			}

			MinorAxisUniformDesiredSize = FMath::Max(MinorAxisUniformDesiredSize, MinorAxisDesiredSize);

		}
	}

	return Orientation == Orient_Horizontal
		? FVector2D(NumUniformCells*MajorAxisUniformDesiredSize + NonUniformChildDesiredSize.X, NumChildren*MinorAxisUniformDesiredSize)
		: FVector2D(NumChildren*MinorAxisUniformDesiredSize, NumUniformCells*MajorAxisUniformDesiredSize + NonUniformChildDesiredSize.Y);
}

FChildren* SUniformToolbarPanel::GetChildren()
{
	return &Children;
}

void SUniformToolbarPanel::SetSlotPadding(TAttribute<FMargin> InSlotPadding)
{
	SlotPadding = InSlotPadding;
	Invalidate(EInvalidateWidgetReason::Layout);
}


SUniformToolbarPanel::FSlot& SUniformToolbarPanel::AddSlot()
{
	FSlot& NewSlot = *(new FSlot());

	Children.Add( &NewSlot );

	return NewSlot;
}

bool SUniformToolbarPanel::RemoveSlot( const TSharedRef<SWidget>& SlotWidget )
{
	for (int32 SlotIdx = 0; SlotIdx < Children.Num(); ++SlotIdx)
	{
		if ( SlotWidget == Children[SlotIdx].GetWidget() )
		{
			Children.RemoveAt(SlotIdx);
			return true;
		}
	}
	
	return false;
}

void SUniformToolbarPanel::ClearChildren()
{
	Children.Empty();
}
