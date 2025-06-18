# backend/message_relay.py
import asyncio
import time
import json
import logging
import os
from datetime import datetime

import nats
from nats.errors import ConnectionClosedError, TimeoutError as NatsTimeoutError, NoServersError

# Assuming SessionLocal, JobOutbox, IngestionJobDB are accessible
from backend.database import SessionLocal
from backend.models import JobOutbox # Still useful for type hinting
from backend.crud import get_unpublished_outbox_events, update_outbox_event_status

# --- Configuration ---
NATS_URL = os.getenv("NATS_URL", "nats://localhost:4222")
POLL_INTERVAL_SECONDS = int(os.getenv("RELAY_POLL_INTERVAL", "10"))
MAX_OUTBOX_FETCH_LIMIT = int(os.getenv("RELAY_FETCH_LIMIT", "100"))

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

async def publish_to_nats(nc, subject: str, payload: bytes, message_id: str):
    if not nc or not nc.is_connected:
        logger.error(f"NATS connection not available for publishing message ID '{message_id}'.")
        return False
    try:
        ack = await nc.publish(subject, payload, headers={'Nats-Msg-Id': message_id})
        logger.info(f"Published message to NATS subject '{subject}', ID '{message_id}'. ACK: stream='{ack.stream}', seq={ack.seq}, duplicate={ack.duplicate}")
        return True
    except NatsTimeoutError:
        logger.error(f"Timeout publishing message ID '{message_id}' to NATS subject '{subject}'.")
    except ConnectionClosedError:
        logger.error(f"NATS connection closed while publishing message ID '{message_id}' to subject '{subject}'.")
    except NoServersError:
        logger.error(f"No NATS servers available to publish message ID '{message_id}' to subject '{subject}'.")
    except Exception as e:
        logger.error(f"Unexpected error publishing message ID '{message_id}' to NATS subject '{subject}': {e}", exc_info=True)
    return False

async def relay_cycle(nc):
    db: SessionLocal = None # type: ignore
    published_this_cycle = 0
    processed_event_ids_in_cycle = [] # To keep track of successfully processed events for commit

    try:
        db = SessionLocal()
        if not db:
            logger.error("Failed to create database session.")
            return 0

        events_to_publish = get_unpublished_outbox_events(db, MAX_OUTBOX_FETCH_LIMIT)

        if not events_to_publish:
            # logger.debug("No unpublished events found in outbox.") # Too verbose for INFO level
            return 0

        logger.info(f"Found {len(events_to_publish)} events to publish.")

        for event_model in events_to_publish:
            event_id = event_model.event_id
            nats_subject = event_model.nats_subject
            nats_payload_bytes = json.dumps(event_model.nats_message_payload).encode('utf-8')
            nats_message_id = event_model.nats_message_id

            success = await publish_to_nats(nc, nats_subject, nats_payload_bytes, nats_message_id)

            if success:
                updated_event = update_outbox_event_status(db, event_id, "PUBLISHED", published_at=datetime.utcnow())
                if updated_event:
                    logger.info(f"Marked event_id: {event_id} as PUBLISHED in DB for job_id: {event_model.job_id}.")
                    processed_event_ids_in_cycle.append(event_id) # Add to list for commit
                    published_this_cycle += 1
                else:
                    logger.error(f"Failed to update status for event_id: {event_id} in DB even after successful NATS publish. This event will not be committed now and might be re-processed.")
                    # This event's DB update failed, so it won't be part of this transaction's commit.
                    # It remains UNPUBLISHED and will be picked up again. If NATS publish was truly successful,
                    # this could lead to duplicate message delivery if the NATS broker didn't deduplicate
                    # based on Nats-Msg-Id on the re-delivery.
            else:
                logger.warning(f"Failed to publish event_id: {event_id} for job_id: {event_model.job_id}. It will be retried in a future cycle.")

        if processed_event_ids_in_cycle: # Only commit if there were successful DB updates
            db.commit()
            logger.info(f"Committed DB updates for {len(processed_event_ids_in_cycle)} published events.")
        # else:
            # logger.info("No events were successfully processed and marked for DB commit in this cycle.")


    except Exception as e:
        logger.error(f"Error during relay cycle: {e}", exc_info=True)
        if db:
            db.rollback()
            logger.info("Database transaction rolled back due to error in relay cycle.")
    finally:
        if db:
            db.close()
    return published_this_cycle

async def run_relay():
    logger.info("Starting Message Relay Service...")
    nc = None

    if not SessionLocal: # Should not happen if imports are correct
        logger.critical("SessionLocal is not available. Cannot connect to database. Exiting.")
        return

    while True:
        try:
            if nc is None or not nc.is_connected:
                if nc and (nc.is_closed or nc.is_draining):
                    logger.info("NATS connection was closed or is draining. Attempting to reconnect...")

                logger.info(f"Attempting to connect to NATS server at {NATS_URL}...")
                nc = await nats.connect(
                    NATS_URL,
                    reconnect_time_wait=5,
                    max_reconnect_attempts=-1,
                    ping_interval=20,
                    max_outstanding_pings=2,
                    error_cb=lambda e: logger.error(f"NATS Error callback: {e}"),
                    disconnected_cb=lambda: logger.warning("NATS Disconnected callback."),
                    reconnected_cb=lambda: logger.info(f"NATS Reconnected callback to {nc.connected_url.netloc if nc and nc.connected_url else 'N/A'}."),
                    closed_cb=lambda: logger.warning("NATS Closed callback.")
                )
                logger.info(f"Successfully connected to NATS at {nc.connected_url.netloc if nc.connected_url else 'N/A'}")

            await relay_cycle(nc)

        except NoServersError:
            logger.error(f"Could not connect to NATS: No servers available at {NATS_URL}. Retrying in {POLL_INTERVAL_SECONDS}s...")
        except ConnectionRefusedError:
            logger.error(f"NATS connection refused at {NATS_URL}. Retrying in {POLL_INTERVAL_SECONDS}s...")
        except Exception as e:
            logger.error(f"Message Relay run_relay loop error: {e}", exc_info=True)
            if nc and nc.is_connected:
                logger.info("Closing NATS connection due to unexpected error in relay loop.")
                await nc.close() # type: ignore
            nc = None

        await asyncio.sleep(POLL_INTERVAL_SECONDS)

if __name__ == "__main__":
    logger.info("Message Relay process started (__main__).")
    try:
        asyncio.run(run_relay())
    except KeyboardInterrupt:
        logger.info("Message Relay Service shutting down gracefully (KeyboardInterrupt)...")
    finally:
        logger.info("Message Relay Service has stopped.")
