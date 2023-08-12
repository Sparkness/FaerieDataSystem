﻿// Copyright Guy (Drakynfly) Lundvall. All Rights Reserved.

#include "Extensions/InventoryLoggerExtension.h"

#include "Net/UnrealNetwork.h"

void UInventoryLoggerExtension::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, EventLog, SharedParams);
}

void UInventoryLoggerExtension::PostAddition(const UFaerieItemContainerBase* Container, const Faerie::FItemContainerEvent& Event)
{
	HandleNewEvent({Container, Event});
}

void UInventoryLoggerExtension::PostRemoval(const UFaerieItemContainerBase* Container, const Faerie::FItemContainerEvent& Event)
{
	HandleNewEvent({Container, Event});
}

void UInventoryLoggerExtension::HandleNewEvent(const FLoggedInventoryEvent& Event)
{
	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, EventLog, this);
	EventLog.Add(Event);
	OnInventoryEventLogged.Broadcast(Event);
}

TArray<FLoggedInventoryEvent> UInventoryLoggerExtension::GetRecentEvents(const int32 NumEvents) const
{
	if (NumEvents >= EventLog.Num())
	{
		return EventLog;
	}

	TArray<FLoggedInventoryEvent> OutEvents;
	for (int32 i = EventLog.Num() - NumEvents; i < EventLog.Num(); ++i)
	{
		OutEvents.Add(EventLog[i]);
	}
	return OutEvents;
}

void UInventoryLoggerExtension::OnRep_EventLog()
{
	// When the EventLog is replicated to clients, we need to check how many events behind we are.
	const int32 BehindCount = EventLog.Num() - LocalEventLogCount;

	auto&& RecentLogs = GetRecentEvents(BehindCount);

	for (auto RecentLog : RecentLogs)
	{
		OnInventoryEventLogged.Broadcast(RecentLog);
	}

	LocalEventLogCount = EventLog.Num();
}

void ULoggedInventoryEventLibrary::BreakLoggedInventoryEvent(const FLoggedInventoryEvent& LoggedEvent, FFaerieInventoryTag& Type,
                                                             bool& Success, FDateTime& Timestamp, FEntryKey& EntryTouched,
                                                             TArray<FFaerieItemKeyBase>& OtherKeysTouched, FFaerieItemStackView& Stack, FString& ErrorMessage)
{
	Type = LoggedEvent.Event.Type;
	Success = LoggedEvent.Event.Success;
	Timestamp = LoggedEvent.Event.GetTimestamp();
	EntryTouched = LoggedEvent.Event.EntryTouched;
	OtherKeysTouched = LoggedEvent.Event.OtherKeysTouched;
	Stack.Copies = LoggedEvent.Event.Amount;
	Stack.Item = LoggedEvent.Event.Item.IsValid() ? LoggedEvent.Event.Item.Get() : nullptr;
	ErrorMessage = LoggedEvent.Event.ErrorMessage;
}
