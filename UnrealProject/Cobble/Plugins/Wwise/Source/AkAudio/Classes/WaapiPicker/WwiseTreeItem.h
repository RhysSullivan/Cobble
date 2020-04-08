// Copyright (c) 2006-2012 Audiokinetic Inc. / All Rights Reserved

/*------------------------------------------------------------------------------------
	WwiseTreeItem.h
------------------------------------------------------------------------------------*/
#pragma once
#include "Engine/GameEngine.h"
#include "Widgets/Views/STableRow.h"
#include "WwiseItemType.h"

/*------------------------------------------------------------------------------------
	WwiseTreeItem
------------------------------------------------------------------------------------*/
struct FWwiseTreeItem : public TSharedFromThis<FWwiseTreeItem>
{
	/** Name to display */
	FString DisplayName;
	/** The path of the tree item including the name */
	FString FolderPath;
	/** Type of the item */
	EWwiseItemType::Type ItemType = EWwiseItemType::None;
	/** Id of the item */
	FGuid ItemId;

	/** The children of this tree item */
	TArray< TSharedPtr<FWwiseTreeItem> > Children;
	
	/** The number of children of this tree item requested from Wwise*/
	uint32_t ChildCountInWwise = 0;

	/** The parent folder for this item */
	TWeakPtr<FWwiseTreeItem> Parent;

	/** The row in the tree view associated to this item */
	TWeakPtr<ITableRow> TreeRow;

	/** Should this item be visible? */
	bool IsVisible = true;

	/** Constructor */
	FWwiseTreeItem(FString InDisplayName, FString InFolderPath, TSharedPtr<FWwiseTreeItem> InParent, EWwiseItemType::Type InItemType, const FGuid& InItemId)
		: DisplayName(MoveTemp(InDisplayName))
		, FolderPath(MoveTemp(InFolderPath))
		, ItemType(MoveTemp(InItemType))
		, ItemId(InItemId)
		, ChildCountInWwise(Children.Num())
        , Parent(MoveTemp(InParent))
        , IsVisible(true)
	{}

	/** Returns true if this item is a child of the specified item */
	bool IsChildOf(const FWwiseTreeItem& InParent)
	{
		auto CurrentParent = Parent.Pin();
		while (CurrentParent.IsValid())
		{
			if (CurrentParent.Get() == &InParent)
			{
				return true;
			}

			CurrentParent = CurrentParent->Parent.Pin();
		}

		return false;
	}

	bool IsOfType(const TArray<EWwiseItemType::Type>& Types)
	{
		for (const auto& Type : Types)
			if (ItemType == Type)
				return true;

		return false;
	}

	bool IsNotOfType(const TArray<EWwiseItemType::Type>& Types)
	{
		for (const auto& Type : Types)
			if (ItemType == Type)
				return false;

		return true;
	}

	/** Returns the child item by name or NULL if the child does not exist */
	TSharedPtr<FWwiseTreeItem> GetChild (const FString& InChildName)
	{
		for ( int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx )
		{
			if ( Children[ChildIdx]->DisplayName == InChildName )
			{
				return Children[ChildIdx];
			}
		}

		return TSharedPtr<FWwiseTreeItem>();
	}

	/** Finds the child who's path matches the one specified */
	TSharedPtr<FWwiseTreeItem> FindItemRecursive (const FString& InFullPath)
	{
		if ( InFullPath == FolderPath )
		{
			return SharedThis(this);
		}

		for ( int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx )
		{
			const TSharedPtr<FWwiseTreeItem>& Item = Children[ChildIdx]->FindItemRecursive(InFullPath);
			if ( Item.IsValid() )
			{
				return Item;
			}
		}

		return TSharedPtr<FWwiseTreeItem>(NULL);
	}

	struct FCompareWwiseTreeItem
	{
		FORCEINLINE bool operator()( TSharedPtr<FWwiseTreeItem> A, TSharedPtr<FWwiseTreeItem> B ) const
		{
			// Items are sorted like so:
			// 1- Physical folders, sorted alphabetically
			// 1- WorkUnits, sorted alphabetically
			// 2- Virtual folders, sorted alphabetically
			// 3- Normal items, sorted alphabetically
			if( A->ItemType == B->ItemType)
			{
				return A->DisplayName < B->DisplayName;
			}
			else if( A->ItemType == EWwiseItemType::PhysicalFolder )
			{
				return true;
			}
			else if( B->ItemType == EWwiseItemType::PhysicalFolder )
			{
				return false;
			}
			else if( A->ItemType == EWwiseItemType::StandaloneWorkUnit || A->ItemType == EWwiseItemType::NestedWorkUnit )
			{
				return true;
			}
			else if( B->ItemType == EWwiseItemType::StandaloneWorkUnit || B->ItemType == EWwiseItemType::NestedWorkUnit )
			{
				return false;
			}
			else if( A->ItemType == EWwiseItemType::Folder )
			{
				return true;
			}
			else if( B->ItemType == EWwiseItemType::Folder )
			{
				return false;
			}
			else
			{
				return true;
			}
		}
	};

	/** Sort the children by name */
	void SortChildren ()
	{
		Children.Sort( FCompareWwiseTreeItem() );
	}
};
