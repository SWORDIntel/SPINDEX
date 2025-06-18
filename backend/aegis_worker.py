# backend/aegis_worker.py
import asyncio
import json
import logging
import os
import signal # For graceful shutdown

import nats
from nats.errors import ConnectionClosedError, TimeoutError as NatsTimeoutError, NoServersError

# --- Backend Imports ---
from backend.database import SessionLocal # For creating DB sessions in the worker
from backend.pipeline_logic import execute_aegis_pipeline # The refactored pipeline logic
from backend.crud import get_job_status # For idempotency checks

# --- Configuration ---
NATS_URL = os.getenv("NATS_URL", "nats://localhost:4222")
AEGIS_JOB_STREAM_NAME = os.getenv("NATS_AEGIS_JOB_STREAM_NAME", "AEGIS_JOB_STREAM")
# AEGIS_JOB_SUBJECT_TO_CONSUME = os.getenv("NATS_AEGIS_JOB_SUBJECT", "aegis.jobs.new") # Not directly used by pull consumer
DURABLE_CONSUMER_NAME = os.getenv("NATS_AEGIS_WORKER_DURABLE_NAME", "aegis_job_worker")

WORKER_FETCH_TIMEOUT_SECONDS = int(os.getenv("WORKER_FETCH_TIMEOUT_SECONDS", "5"))
WORKER_POLL_INTERVAL_SECONDS = int(os.getenv("WORKER_POLL_INTERVAL_SECONDS", "1"))

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

shutdown_event = asyncio.Event()

def handle_shutdown_signal(sig, frame):
    logger.info(f"Received shutdown signal {sig}. Initiating graceful shutdown...")
    shutdown_event.set()

async def run_worker():
    logger.info("Starting AEGIS Worker Service...")
    nc = None
    js = None

    while not shutdown_event.is_set():
        db_session_outer_loop = None # For NATS connection related DB ops if any, though not used currently
        try:
            if nc is None or not nc.is_connected:
                logger.info(f"Attempting to connect to NATS server at {NATS_URL}...")
                nc = await nats.connect(
                    NATS_URL,
                    reconnect_time_wait=5, max_reconnect_attempts=-1,
                    ping_interval=20, max_outstanding_pings=2,
                    error_cb=lambda e: logger.error(f"NATS Error callback: {e}"),
                    disconnected_cb=lambda: logger.warning("NATS Disconnected callback."),
                    reconnected_cb=lambda: logger.info(f"NATS Reconnected callback to {nc.connected_url.netloc if nc and nc.connected_url else 'N/A'}."), # type: ignore
                    closed_cb=lambda: logger.warning("NATS Closed callback.")
                )
                js = nc.jetstream()
                logger.info(f"Successfully connected to NATS at {nc.connected_url.netloc if nc.connected_url else 'N/A'}. JetStream context obtained.") # type: ignore

            psub = await js.pull_subscribe( # type: ignore
                subject="",
                durable=DURABLE_CONSUMER_NAME,
                stream=AEGIS_JOB_STREAM_NAME
            )
            logger.info(f"Pull consumer '{DURABLE_CONSUMER_NAME}' listening on stream '{AEGIS_JOB_STREAM_NAME}'.")

            while not shutdown_event.is_set():
                db_session_inner_loop = None # For individual message processing DB ops
                msg = None # Ensure msg is defined for potential error logging outside try-except for msgs
                try:
                    msgs = await psub.fetch(1, timeout=WORKER_FETCH_TIMEOUT_SECONDS)
                    if msgs:
                        msg = msgs[0]
                        logger.info(f"WORKER: Received message (seq: {msg.seq}, subject: {msg.subject}, data: {msg.data.decode()})")

                        job_id = "unknown_job_id"
                        try:
                            data = json.loads(msg.data.decode())
                            job_id = data.get("job_id")

                            if not job_id: # Check if job_id is None or empty string
                                logger.error(f"WORKER: Received malformed message - job_id is missing or empty: {data}")
                                await msg.term() # Terminate message, won't be redelivered.
                                continue # Process next message

                            logger.info(f"WORKER: Processing job_id: {job_id} received from NATS.")
                            should_process_job = True

                            db_session_inner_loop = SessionLocal()
                            if not db_session_inner_loop:
                                logger.error(f"WORKER: Failed to create DB session for job {job_id}. Retrying message later.")
                                # Let message redeliver by not ACKing
                                await asyncio.sleep(WORKER_POLL_INTERVAL_SECONDS)
                                continue

                            # --- Idempotency Check ---
                            current_job_status = get_job_status(db_session_inner_loop, job_id)
                            if current_job_status:
                                logger.info(f"WORKER: Current status for job {job_id} is '{current_job_status}'.")
                                terminal_or_active_states = [
                                    "COMPLETED", "PROCESSING", "VALIDATION_FAILED", "MODULE_FAILED",
                                    "QUARANTINED", "failed_", # Catches all "failed_..." statuses
                                    # Add specific "completed_" statuses from pipeline_logic if needed for more granularity
                                    "completed_format_conversion", "completed_crs_detection", "completed_crs_transformation",
                                    "completed_geocoding", "completed_complex_geometry"
                                ]
                                if any(state_fragment in current_job_status for state_fragment in terminal_or_active_states):
                                    logger.info(f"WORKER: Job {job_id} is in a terminal or active state ('{current_job_status}'). Skipping re-processing. Acknowledging message.")
                                    should_process_job = False
                                    await msg.ack()
                            else:
                                logger.error(f"WORKER: Job {job_id} not found in database for idempotency check. Terminating message.")
                                should_process_job = False
                                await msg.term()

                            if not should_process_job:
                                continue # Go to next message fetch

                            # --- Actual Processing ---
                            processing_success = False
                            if should_process_job: # Double check, though logic above should ensure this
                                logger.info(f"WORKER: Invoking execute_aegis_pipeline for job {job_id}.")
                                loop = asyncio.get_running_loop()
                                processing_success = await loop.run_in_executor(None, execute_aegis_pipeline, job_id, db_session_inner_loop, logger)
                                logger.info(f"WORKER: `execute_aegis_pipeline` for job {job_id} returned: {processing_success}")

                            # --- Message Acknowledgment Logic ---
                            if processing_success:
                                await msg.ack()
                                logger.info(f"WORKER: Message for job {job_id} acknowledged after successful pipeline execution.")
                            else:
                                # Pipeline returned False, implying it handled its own error state and DB update.
                                # Or an exception occurred during run_in_executor but before pipeline_logic could set a terminal state.
                                final_job_status_after_failure = get_job_status(db_session_inner_loop, job_id)
                                logger.warning(f"WORKER: Pipeline execution indicated failure for job {job_id}. Final DB status: '{final_job_status_after_failure}'.")
                                if final_job_status_after_failure and any(state_fragment in final_job_status_after_failure for state_fragment in ["FAILED", "QUARANTINED", "COMPLETED"]):
                                    await msg.ack() # Pipeline set a terminal state
                                    logger.info(f"WORKER: Message for job {job_id} ACKed as pipeline set terminal status '{final_job_status_after_failure}'.")
                                else:
                                    logger.warning(f"WORKER: Job {job_id} pipeline processing failed but status '{final_job_status_after_failure}' is not terminal. Message will NOT be ACKed for NATS redelivery.")
                                    # Consider NAK for more control: await msg.nak(delay=60)

                        except json.JSONDecodeError:
                            logger.error(f"WORKER: Failed to decode JSON from message: {msg.data.decode() if msg else 'N/A'}")
                            if msg: await msg.term()
                        except Exception as e_proc: # Catch errors within individual message processing
                            logger.error(f"WORKER: Unhandled error processing message for job {job_id}: {e_proc}", exc_info=True)
                            # Default: Do not ACK, let NATS redeliver. Consider if TERM is more appropriate for some errors.
                            # If msg is available and not yet acked/termed, this implies a redelivery or manual check.
                        finally:
                            if db_session_inner_loop:
                                db_session_inner_loop.close()
                    else: # No messages from fetch
                        await asyncio.sleep(WORKER_POLL_INTERVAL_SECONDS)

                except NatsTimeoutError: # Timeout on psub.fetch()
                    await asyncio.sleep(0.1)
                    continue
                except ConnectionClosedError: # NATS connection closed during fetch or psub interaction
                    logger.warning("NATS connection closed during inner loop. Will attempt to reconnect in outer loop.")
                    break # Break inner loop to trigger outer loop's reconnect logic
                except Exception as e_inner_loop: # Other errors in the consumption loop
                    logger.error(f"Error in worker's message consumption loop: {e_inner_loop}", exc_info=True)
                    break # Break inner loop, will attempt to re-establish NATS connection/consumer

        except NoServersError:
            logger.error(f"Could not connect to NATS: No servers available at {NATS_URL}. Retrying in {WORKER_POLL_INTERVAL_SECONDS}s...")
        except ConnectionRefusedError:
            logger.error(f"NATS connection refused at {NATS_URL}. Retrying in {WORKER_POLL_INTERVAL_SECONDS}s...")
        except Exception as e_outer_loop:
            logger.error(f"AEGIS Worker run_worker outer loop error: {e_outer_loop}", exc_info=True)
        finally:
            if nc and nc.is_connected:
                logger.info("Closing NATS connection due to worker stopping or error in outer loop.")
                await nc.close()
            nc = None
            js = None
            if not shutdown_event.is_set():
                await asyncio.sleep(WORKER_POLL_INTERVAL_SECONDS)

    logger.info("AEGIS Worker Service has shut down.")

if __name__ == "__main__":
    signal.signal(signal.SIGINT, handle_shutdown_signal)
    signal.signal(signal.SIGTERM, handle_shutdown_signal)
    logger.info("AEGIS Worker process started (__main__). Press Ctrl+C to exit.")
    try:
        asyncio.run(run_worker())
    except KeyboardInterrupt:
        logger.info("AEGIS Worker Service shutting down (KeyboardInterrupt in main)...")
    finally:
        logger.info("AEGIS Worker Service has stopped.")
