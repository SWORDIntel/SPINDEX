"""
Telegram Autobot (Python) with Enhanced Error Handling
------------------------------------------------------
"""

import os
import json
import time
import asyncio
import subprocess
import re
import logging # ENHANCEMENT: Using Python's logging module
from datetime import datetime, timedelta
from collections import defaultdict
from typing import Dict, Any, List, Optional, Tuple # ENHANCEMENT: Added some types

import requests
import openai
from telethon import TelegramClient, events
from telethon.errors import SessionPasswordNeededError
from dotenv import load_dotenv

# ----------------------------------------------------------------------
# Logging Configuration
# ----------------------------------------------------------------------
# ENHANCEMENT: Setup basic logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s - %(levelname)s - %(name)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger(__name__)

# ----------------------------------------------------------------------
# Load environment variables
# ----------------------------------------------------------------------
load_dotenv()

TG_API_ID = os.getenv("TG_API_ID")
TG_API_HASH = os.getenv("TG_API_HASH")
PHONE_NUMBER = os.getenv("PHONE_NUMBER")
SESSION_NAME = os.getenv("SESSION_NAME", "session")
MY_USERNAME = os.getenv("MY_USERNAME")
LLM_CONTEXT_PATH = os.getenv("LLM_CONTEXT_PATH")
COMMAND_BASE_URL = os.getenv("COMMAND_BASE_URL")
LLMM1 = os.getenv("LLMM1")
OZZO_API_KEY = os.getenv("OZZO_API_KEY")
OZZO_GETCONT = os.getenv("OZZO_GETCONT")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY", "")
UNCENSORED_AI_URL = os.getenv("UNCENSORED_AI_URL", "http://0.0.0.0:1234")
ZINDEX_PATH = os.getenv("ZINDEX_PATH", "./zindex5") # ENHANCEMENT: Configurable zindex path

AUTHORIZED_IDS = [
    "<REDACTED>" # IMPORTANT: Replace with actual numeric Telegram User IDs
]
UNAUTHORIZED_IDS = [ # For specific commands like /z
    "<REDACTED>"
]

# ENHANCEMENT: Startup check for AUTHORIZED_IDS
if not AUTHORIZED_IDS or AUTHORIZED_IDS == ["<REDACTED>"]:
    logger.critical("CRITICAL: AUTHORIZED_IDS is not configured. "
                    "Please edit the script and add valid user IDs.")
    exit(1)

# ENHANCEMENT: More explicit check for required environment variables
REQUIRED_ENV_VARS = [
    ("TG_API_ID", TG_API_ID),
    ("TG_API_HASH", TG_API_HASH),
    ("PHONE_NUMBER", PHONE_NUMBER),
    ("MY_USERNAME", MY_USERNAME),
    ("LLM_CONTEXT_PATH", LLM_CONTEXT_PATH), # Note: Script handles its absence gracefully for !cb
    ("COMMAND_BASE_URL", COMMAND_BASE_URL), # For !q, !b
    ("OPENAI_API_KEY", OPENAI_API_KEY),   # For !cai (OpenAI summary)
    # Variables for specific commands; if not set, those commands might fail
    # ("LLMM1", LLMM1), # For !q if LLMM1 model is used
    # ("OZZO_API_KEY", OZZO_API_KEY), # For !w if OZZO is used
    # ("OZZO_GETCONT", OZZO_GETCONT), # For !w if OZZO is used
    # ("UNCENSORED_AI_URL", UNCENSORED_AI_URL), # For !cai (uncensored summary)
    # ("ZINDEX_PATH", ZINDEX_PATH) # For /z
]
missing_vars = [name for name, value in REQUIRED_ENV_VARS if value is None]
if any(name for name, value in REQUIRED_ENV_VARS[:4] if value is None): # Core Telegram vars must exist
    logger.critical(f"Missing critical Telegram environment variables: "
                    f"{[name for name, value in REQUIRED_ENV_VARS[:4] if value is None]}. "
                    "Check your .env file.")
    exit(1)
if missing_vars:
    logger.warning(f"Potentially missing environment variables: {', '.join(missing_vars)}. "
                   "Some features might not work as expected. Check your .env file.")


# ----------------------------------------------------------------------
# Constants & Initialization
# ----------------------------------------------------------------------
LOGS_DIR = os.getenv("LOGS_DIR", "./logs") # Allow overriding LOGS_DIR via .env
MAX_UNIQUE_LINES = int(os.getenv("MAX_UNIQUE_LINES", 500))
MAX_LOGIN_ATTEMPTS = int(os.getenv("MAX_LOGIN_ATTEMPTS", 3))
SAVE_INTERVAL_SECONDS = int(os.getenv("SAVE_INTERVAL_SECONDS", 5 * 60))

messages_map: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
entity_cache: Dict[int, str] = {}

# Initialize Telethon client
client = TelegramClient(SESSION_NAME, int(TG_API_ID) if TG_API_ID else 0, TG_API_HASH if TG_API_HASH else "")

# Configure OpenAI
if OPENAI_API_KEY:
    openai.api_key = OPENAI_API_KEY
else:
    logger.warning("OPENAI_API_KEY is not set. OpenAI dependent features (e.g., default !cai) will not work.")


# ENHANCEMENT: Added 'description' for !help command
COMMANDS: Dict[str, Dict[str, Any]] = {
    "q": {
        "prefix": "!q",
        "endpoint": f"{COMMAND_BASE_URL}/q" if COMMAND_BASE_URL else None,
        "params": {
            "temperature": 0.3,
            "tokens": 3000,
            "model": LLMM1,
            "version": "v2",
        },
        "description": "Query a general-purpose LLM. Usage: `!q <your question>`"
    },
    "b": {
        "prefix": "!cb", # cb often means "context bot" or "chat bot"
        "endpoint": f"{COMMAND_BASE_URL}/q" if COMMAND_BASE_URL else None,
        "params": {
            "temperature": 0.92,
            "tokens": 4000,
            "format": "fun",
            "model": "openai-next", # This seems like a placeholder/example
            "version": "v2",
        },
        "description": "Query an LLM with additional context (from `LLM_CONTEXT_PATH`). Usage: `!cb <your question>`"
    },
    "w": {
        "prefix": "!w",
        "endpoint": OZZO_GETCONT,
        "params": { # These params are specific to how send_prompt_and_get_response handles "w"
            "headers": {
                "X-API-KEY": OZZO_API_KEY,
                "Content-Type": "application/json",
            },
        },
        "description": "Fetch web content. Flags: `+pdf`, `+inline`. Usage: `!w [+pdf] [+inline] <url>`"
    },
    "arf": {
        "prefix": "!arf",
        "description": "Replies with ARF! ARF! ARF!"
    },
    "summarize": {
        "prefix": "!cai", # cai often means "chat ai" or "context ai"
        "description": "Summarize recent chat messages. Flag: `-d N` (days ago). Usage: `!cai [-d N]`"
    },
    "search": {
        "prefix": "!search",
        "description": "Search logged messages in this chat. Usage: `!search <keyword>`"
    },
    "stats": {
        "prefix": "!stats",
        "description": "Show statistics for today's messages in this chat."
    },
    "fsearch": {
        "prefix": "!fsearch",
        "description": "Fuzzy search logged messages in this chat. Usage: `!fsearch <query>`"
    },
    "z": {
        "prefix": "/z",
        "description": f"Search an external index using `{ZINDEX_PATH}`. Usage: `/z <search-term>`"
    },
    "help": { # ENHANCEMENT: Added help command
        "prefix": "!help",
        "description": "Show this help message."
    }
}


# ----------------------------------------------------------------------
# Utility Functions
# ----------------------------------------------------------------------
def is_user_authorized(user_id: int) -> bool:
    """Checks if a given user ID is in the authorized list."""
    return str(user_id) in AUTHORIZED_IDS

def slugify(text: str) -> str:
    """Slugify a text string (lowercase, remove special chars, etc.)."""
    import unicodedata # Keep import local if only used here
    text_norm = unicodedata.normalize("NFD", text)
    text_clean = text_norm.encode("ascii", "ignore").decode("utf-8")
    text_clean = re.sub(r"[^a-zA-Z0-9]+", "-", text_clean.lower())
    return text_clean.strip("-")

def fuzzy_score(text: str, query: str) -> float:
    """Returns ratio of matched characters to query length."""
    t = text.lower()
    q = query.lower()

    t_idx, q_idx, matches = 0, 0, 0
    while t_idx < len(t) and q_idx < len(q):
        if t[t_idx] == q[q_idx]:
            matches += 1
            q_idx += 1
        t_idx += 1

    if len(q) == 0:
        return 0.0
    return matches / len(q)

def parse_command(text: str) -> Optional[Dict[str, str]]:
    """Parses text to see if it matches any known command prefix."""
    if not text:
        return None
    for key, value in COMMANDS.items():
        if text.startswith(value["prefix"]):
            content = text[len(value["prefix"]):].strip()
            return {"command": key, "content": content}
    return None


# ----------------------------------------------------------------------
# LLM / AI Request Helpers with Improved Error Handling
# ----------------------------------------------------------------------
async def send_prompt_and_get_response(command: str, content: str, context: str = "") -> str | Dict[str, Any]:
    """
    Posts content+context to the configured LLM endpoint.
    Returns either a string or an object with file data.
    Now includes robust error handling and fallback responses.
    """
    cmd_config = COMMANDS.get(command, {})
    endpoint = cmd_config.get("endpoint") # Removed default ""
    params = cmd_config.get("params", {})

    if not endpoint:
        logger.error(f"No endpoint configured for command '{command}'.")
        return f"Error: No endpoint configured for command '{command}'."

    try:
        if command == "w":
            # OZZO content fetch
            headers = params.get("headers", {})
            if not OZZO_API_KEY and "X-API-KEY" in headers: # Check if API key is actually needed and missing
                 logger.warning("OZZO_API_KEY is not set, but 'X-API-KEY' is in headers for !w command.")
                 # Potentially remove X-API-KEY from headers if None to avoid sending empty header
                 # Or, make OZZO_API_KEY strictly required if this path is taken. For now, let it try.
            payload = {"url": content}
            try:
                resp = requests.post(endpoint, json=payload, headers=headers, timeout=15)
                resp.raise_for_status()
            except requests.exceptions.HTTPError as e:
                logger.error(f"HTTP error fetching content for !w: {e.response.status_code} - {e.response.text}")
                return f"Sorry, there was an HTTP error fetching content: {e.response.status_code}"
            except requests.exceptions.RequestException as e:
                logger.error(f"Network error fetching content for !w: {e}")
                return f"Network error fetching content: {e}"

            try:
                data = resp.json()
                text_md = data.get("markdown", "")
                if not text_md:
                    logger.warning("No markdown data found in response from !w endpoint.")
                    return "No markdown data found in response."
                return {
                    "name": slugify(content),
                    "data": text_md.encode("utf-8"),
                    "text": text_md
                }
            except (ValueError, KeyError) as e:
                logger.error(f"Error parsing JSON response from !w endpoint: {e}")
                return f"Error parsing response JSON: {e}"

        # Otherwise, generic LLM endpoint
        payload = {
            "question": content,
            "context": context,
            **params.get("params", params) # Original script had params directly, now it might be nested if I change COMMANDS
        }
        # Ensure model is part of payload if available in cmd_config (especially for !q)
        if "model" in cmd_config.get("params", {}):
            payload["model"] = cmd_config["params"]["model"]


        try:
            resp = requests.post(endpoint, json=payload, headers={"Content-Type": "application/json"}, timeout=20)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as e:
            logger.error(f"HTTP error from AI service for command {command}: {e.response.status_code} - {e.response.text}")
            return f"HTTP error from AI service ({e.response.status_code})."
        except requests.exceptions.RequestException as e:
            logger.error(f"Network error from AI service for command {command}: {e}")
            return f"Network error connecting to AI service: {e}"

        try:
            data = resp.json()
        except ValueError:
            logger.error(f"Failed to parse AI response (invalid JSON) for command {command}.")
            return "Error: Failed to parse AI response (invalid JSON)."

        assistant_msg = data.get("assistant", data.get("answer", "")) # Check for common response keys
        if not assistant_msg and "choices" in data and data["choices"]: # OpenAI like structure
            assistant_msg = data["choices"][0].get("message", {}).get("content", "")
        
        if not assistant_msg:
            logger.warning(f"AI returned an empty or unparseable response for command {command}. Full response: {data}")
            return "AI returned an empty or unparseable response."
        return assistant_msg

    except Exception as e:
        logger.exception(f"Sorry, an unexpected error occurred in send_prompt_and_get_response for command {command}: {e}")
        return f"Sorry, an unexpected error occurred: {e}"

def convert_markdown_to_pdf(markdown: str) -> bytes:
    """
    Converts markdown text into PDF (calling a remote service).
    If the service is not available or returns invalid data, returns empty bytes.
    """
    params = {"markdown": markdown}
    pdf_service_url = "https://md-to-pdf.fly.dev" # Make this configurable if needed
    try:
        r = requests.post(pdf_service_url, data=params, timeout=15)
        r.raise_for_status()
        return r.content
    except requests.exceptions.RequestException as e:
        logger.error(f"convert_markdown_to_pdf (RequestException): {e}")
        return b""
    except Exception as e:
        logger.error(f"convert_markdown_to_pdf (Other Exception): {e}")
        return b""


# ----------------------------------------------------------------------
# Summaries with OpenAI or "Uncensored" Endpoint, with Error Handling
# ----------------------------------------------------------------------
async def get_chat_summary(chat_text: str) -> str:
    """
    Summarize chat text using official OpenAI GPT endpoints.
    In case of error or empty response, fallback gracefully.
    """
    if not OPENAI_API_KEY:
        return "OpenAI API key not configured. Cannot generate summary."
    if not chat_text.strip():
        return "No chat data to summarize."

    try:
        # Updated to new OpenAI client syntax if openai version >= 1.0
        if hasattr(openai, "chat") and hasattr(openai.chat, "completions"): # Check for v1.x.x
            completion = openai.chat.completions.create(
                model="gpt-3.5-turbo",
                messages=[
                    {"role": "system", "content": "Provide a concise summary of the following chat."},
                    {"role": "user", "content": chat_text}
                ],
                max_tokens=200,
                temperature=0.3,
            )
            content = completion.choices[0].message.content.strip()
        else: # Fallback to older < v1.0.0 syntax
            resp = openai.ChatCompletion.create(
                model="gpt-3.5-turbo",
                messages=[
                    {"role": "system", "content": "Provide a concise summary of the following chat."},
                    {"role": "user", "content": chat_text}
                ],
                max_tokens=200,
                temperature=0.3,
            )
            content = resp.choices[0].message.content.strip()

        if not content:
            logger.warning("OpenAI returned no summary content.")
            return "OpenAI returned no summary."
        return content
    except openai.APIError as e: # Updated error type for v1.x.x
        logger.error(f"OpenAI API Error: {e}")
        return f"OpenAI API Error: {e}"
    except Exception as e: # Catch any other openai error or general exception
        logger.error(f"Error summarizing chat with OpenAI: {e}")
        return f"Error summarizing chat: {e}"

async def get_chat_summary_uncensored(chat_text: str) -> str:
    """
    Summarize chat with an alternative local or uncensored model endpoint.
    Returns fallback message if service is unavailable or data is invalid.
    """
    if not UNCENSORED_AI_URL or UNCENSORED_AI_URL == "http://0.0.0.0:1234" and not os.getenv("UNCENSORED_AI_URL_FORCE_DEFAULT"):
        # A simple check to see if the default is used without explicit intention
        is_default_and_not_forced = (UNCENSORED_AI_URL == "http://0.0.0.0:1234" and
                                     not os.getenv("UNCENSORED_AI_URL_FORCE_DEFAULT"))
        if is_default_and_not_forced and not OPENAI_API_KEY: # If no OpenAI and no real uncensored URL
             logger.info("UNCENSORED_AI_URL is default and not forced, and no OpenAI key. Skipping uncensored summary.")
             return "Uncensored AI endpoint not configured for summary."
        elif is_default_and_not_forced and OPENAI_API_KEY: # Fallback to OpenAI if uncensored is default and not forced
            logger.info("UNCENSORED_AI_URL is default/not explicitly set, falling back to OpenAI for summary.")
            return await get_chat_summary(chat_text)
        elif not UNCENSORED_AI_URL:
            return "Uncensored AI endpoint not configured for summary."


    if not chat_text.strip():
        return "No chat data to summarize."

    payload = {
        "model": "solar-10.7b-instruct-v1.0-uncensored", # This model might need to be configurable
        "messages": [
            {
                "role": "system",
                "content": (
                    "You're an FBI agent who is a mole inside a private chat. "
                    "When prompted, write a detailed summary of the provided messages."
                )
            },
            {
                "role": "user",
                "content": chat_text
            },
        ],
        "max_tokens": 800,
        "temperature": 0.6,
        "stream": False
    }

    try:
        # Assuming Ollama or compatible OpenAI API format
        r = requests.post(f"{UNCENSORED_AI_URL.rstrip('/')}/v1/chat/completions",
                          json=payload, timeout=30) # Increased timeout
        r.raise_for_status()
        data = r.json()
        choices = data.get("choices", [])
        if not choices:
            logger.warning("No choices returned by uncensored model.")
            return "No summary returned by uncensored model."
        
        message_content = choices[0].get("message", {}).get("content", "")
        return message_content.strip() if message_content.strip() else "Empty content from uncensored model."

    except requests.exceptions.HTTPError as e:
        logger.error(f"Uncensored model HTTP error: {e.response.status_code} - {e.response.text}")
        return f"Uncensored model HTTP error: {e.response.status_code}"
    except requests.exceptions.RequestException as e:
        logger.error(f"Uncensored model network error: {e}")
        return f"Uncensored model network error: {e}"
    except (ValueError, KeyError, IndexError) as e:
        logger.error(f"Uncensored model response parse error: {e}")
        return f"Uncensored model response parse error: {e}"
    except Exception as e:
        logger.exception(f"Unhandled error from uncensored model: {e}") # Use .exception to log stack trace
        return f"Unhandled error from uncensored model: {e}"


# ----------------------------------------------------------------------
# Logging Helpers
# ----------------------------------------------------------------------
def read_log_file(filepath: str) -> List[Dict[str, Any]]:
    """Reads a JSON file of messages, returns list or empty."""
    if not os.path.exists(filepath):
        return []
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        logger.error(f"JSONDecodeError reading {filepath}: {e}. Returning empty list.")
        # Optionally, move/backup corrupted file
        # os.rename(filepath, f"{filepath}.corrupted.{int(time.time())}")
        return []
    except Exception as e:
        logger.error(f"Error reading {filepath}: {e}. Returning empty list.")
        return []

def write_log_file(filepath: str, messages: List[Dict[str, Any]]):
    """Writes messages list as JSON."""
    try:
        # Create directory if it doesn't exist
        os.makedirs(os.path.dirname(filepath), exist_ok=True)
        data_str = json.dumps(messages, ensure_ascii=False, indent=2)
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(data_str)
    except Exception as e:
        logger.error(f"Error writing {filepath}: {e}")
        raise # Re-raise to be caught by save_messages_to_file

# ENHANCEMENT: More robust save_messages_to_file
async def save_messages_to_file():
    """Periodically saves unsaved messages from memory to disk."""
    while True:
        await asyncio.sleep(SAVE_INTERVAL_SECONDS)
        for filename_key in list(messages_map.keys()): # Iterate over keys copy
            messages_to_write_snapshot = []
            if messages_map[filename_key]: # Check if there are messages for this key
                messages_to_write_snapshot = list(messages_map[filename_key]) # Take a snapshot
                messages_map[filename_key] = [] # Clear the list for new incoming messages immediately

            if messages_to_write_snapshot:
                filepath = os.path.join(LOGS_DIR, filename_key)
                logger.info(f"Attempting to save {len(messages_to_write_snapshot)} messages to {filepath}...")
                try:
                    existing_messages = read_log_file(filepath)
                    # Prevent duplicates if re-queuing happened and some messages were already written
                    # This is a simple check; more robust would involve message IDs if available and unique
                    existing_ids = {msg.get("messageId") for msg in existing_messages if msg.get("messageId")}
                    
                    unique_new_messages = []
                    if existing_ids:
                        for msg in messages_to_write_snapshot:
                            if msg.get("messageId") not in existing_ids:
                                unique_new_messages.append(msg)
                            else:
                                logger.debug(f"Skipping duplicate messageId {msg.get('messageId')} for {filepath}")
                    else: # No existing IDs or no messageId in existing logs, append all
                        unique_new_messages = messages_to_write_snapshot

                    if not unique_new_messages and messages_to_write_snapshot:
                        logger.info(f"All {len(messages_to_write_snapshot)} messages for {filepath} were already present or had no ID; nothing new to write from this batch.")
                        continue # Skip writing if all were duplicates

                    merged_messages = existing_messages + unique_new_messages
                    write_log_file(filepath, merged_messages)
                    logger.info(f"Successfully saved {len(unique_new_messages)} new messages to {filepath}.")
                except Exception as e:
                    # This 'e' would be from write_log_file or os.makedirs
                    logger.error(f"Failed to save messages to {filepath}. Re-queueing {len(messages_to_write_snapshot)} messages. Error: {e}")
                    # Add messages back to the beginning of the list in messages_map for this file
                    messages_map[filename_key] = messages_to_write_snapshot + messages_map[filename_key]


# ----------------------------------------------------------------------
# Command Handlers
# ----------------------------------------------------------------------
async def handle_help_command(event): # ENHANCEMENT: New command handler
    """Handles the !help command."""
    help_text = "**Available Commands:**\n\n"
    for cmd_key, cmd_data in COMMANDS.items():
        help_text += f"`{cmd_data['prefix']}` - {cmd_data.get('description', 'No description available.')}\n"
    await event.reply(help_text)

async def handle_summarize_command(entity_id, entity_name_or_id, event):
    msg_text = event.raw_text
    args = msg_text.split()
    days_ago = 0

    if "-d" in args:
        try:
            idx = args.index("-d")
            if idx + 1 < len(args):
                days_ago = int(args[idx + 1])
                if days_ago < 0: days_ago = 0 # Ensure non-negative
        except (ValueError, IndexError):
            await event.reply("Invalid format for `-d` flag. Use `-d <number_of_days>`.")
            return
        except Exception: # Catch any other conversion error
            days_ago = 0 # Default if parsing fails

    target_date = datetime.utcnow() - timedelta(days=days_ago)
    # Slugify entity_name_or_id for filename consistency
    filename_slug = slugify(str(entity_name_or_id)) if entity_name_or_id else str(entity_id)
    filename_for_sum = f"{filename_slug}-{target_date.strftime('%Y-%m-%d')}.json"

    file_path = os.path.join(LOGS_DIR, filename_for_sum)
    file_msgs = read_log_file(file_path)
    # For summarization, we might also want to include messages from the current in-memory buffer
    # if we are summarizing "today" (days_ago == 0)
    mem_msgs = []
    if days_ago == 0:
         # current_date_for_mem_key = datetime.utcnow().strftime("%Y-%m-%d") # Use consistent slug
         current_filename_key_for_mem = f"{filename_slug}-{datetime.utcnow().strftime('%Y-%m-%d')}.json"
         mem_msgs = messages_map.get(current_filename_key_for_mem, [])


    combined = file_msgs + mem_msgs # Combine disk and memory (for today)
    if not combined:
        await event.reply(f"No messages found to summarize for {target_date.strftime('%Y-%m-%d')}.")
        return

    # Consider how many messages to summarize. Last 200 might be too few/many.
    # Perhaps summarize a time window or token count if possible.
    recent_msgs = combined[-200:] # Take last 200 messages from the combined list
    chat_text = "\n".join(
        f"{m.get('senderUsername') or m.get('senderFirstName') or 'User'}: {m.get('text')}"
        for m in recent_msgs if m.get('text') # Ensure text exists
    ).strip()

    if not chat_text:
        await event.reply("Found messages, but no actual text content to summarize.")
        return

    await event.reply(f"Summarizing messages from {target_date.strftime('%Y-%m-%d')} ({len(recent_msgs)} messages)...")
    summary = await get_chat_summary_uncensored(chat_text) # Prefers uncensored, falls back if configured
    await event.reply(f"**Summary for {target_date.strftime('%Y-%m-%d')}:**\n{summary}")

async def handle_stats_command(entity_id, filename_key, event): # filename_key is the key for messages_map
    file_path = os.path.join(LOGS_DIR, filename_key)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map.get(filename_key, []) # Use .get for safety
    all_msgs = file_msgs + mem_msgs

    if not all_msgs:
        await event.reply(f"No messages logged yet for the current period ({filename_key}).")
        return

    # --- Basic stats ---
    total_messages = len(all_msgs)
    participants = set()
    for m in all_msgs:
        name = m.get("senderUsername") or m.get("senderFirstName") or "User"
        participants.add(name)
    unique_participants = len(participants)

    # --- Top participants ---
    user_count = defaultdict(int)
    for m in all_msgs:
        name = m.get("senderUsername") or m.get("senderFirstName") or "User"
        user_count[name] += 1
    sorted_users = sorted(user_count.items(), key=lambda x: x[1], reverse=True)
    top_users_str = "\n".join(f"  - {u}: {c} msgs" for u, c in sorted_users[:5])

    # --- Message length stats ---
    total_len = sum(len(m.get("text", "")) for m in all_msgs)
    avg_len = round(total_len / total_messages, 2) if total_messages else 0

    # --- Top words ---
    word_freq = defaultdict(int)
    for m in all_msgs:
        txt = m.get("text", "")
        words = re.split(r"[\s,!.?;:()\"'-]+", txt.lower()) # Improved word splitting
        for w in words:
            if w and len(w) > 2: # Ignore very short words
                word_freq[w] += 1
    top_word_entries = sorted(word_freq.items(), key=lambda x: x[1], reverse=True)[:10]
    top_words_str = ", ".join([f"{w} ({c})" for w, c in top_word_entries]) if top_word_entries else "N/A"

    # --- Activity by hour ---
    hours = [0]*24
    for m in all_msgs:
        d_str = m.get("date")
        if d_str:
            try:
                # Handle potential timezone info (Z or +HH:MM)
                dt_obj = datetime.fromisoformat(d_str.replace("Z", "+00:00"))
                hours[dt_obj.hour] += 1 # Assumes UTC or consistent timezone in logs
            except ValueError:
                logger.warning(f"Could not parse date for stats: {d_str}")
    hour_list = [{"hour": i, "count": c} for i, c in enumerate(hours) if c > 0] # Only list active hours
    hour_list.sort(key=lambda x: x["count"], reverse=True)
    top_hours_str = "\n".join(f"  - {h['hour']:02d}:00: {h['count']} msgs" for h in hour_list[:3]) if hour_list else "N/A"

    # --- Top commands ---
    cmd_counts = defaultdict(int)
    for m in all_msgs:
        t = m.get("text", "").strip()
        if t.startswith("!") or t.startswith("/"):
            cmd = t.split()[0]
            cmd_counts[cmd] += 1
    sorted_cmds = sorted(cmd_counts.items(), key=lambda x: x[1], reverse=True)[:5]
    top_cmds_str = ", ".join([f"{cmd} ({count})" for cmd, count in sorted_cmds]) if sorted_cmds else "N/A"

    stats_msg = (
        f"📊 **Chat Stats for {filename_key.replace('.json', '')}**:\n"
        f"-------------------------------------\n"
        f"• Total Messages: `{total_messages}`\n"
        f"• Unique Participants: `{unique_participants}`\n"
        f"• Top Participants:\n{top_users_str or '  - No messages yet.'}\n\n"
        f"• Avg. Message Length: `{avg_len}` chars\n"
        f"• Top Words (freq):\n  `{top_words_str}`\n\n"
        f"• Busiest Hours (UTC):\n{top_hours_str}\n\n"
        f"• Top Commands Used:\n  `{top_cmds_str}`"
    )
    await event.reply(stats_msg)

async def handle_search_command(entity_id, filename_key, query, event):
    if not query:
        await event.reply("Please provide a search keyword. Usage: `!search <keyword>`")
        return

    file_path = os.path.join(LOGS_DIR, filename_key)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map.get(filename_key, [])
    all_msgs = file_msgs + mem_msgs

    matches = [m for m in all_msgs if m.get("text") and query.lower() in m.get("text", "").lower()]
    
    if not matches:
        await event.reply(f'No matches found for "{query}".')
        return

    # Sort matches by date (newest first) for recent display
    matches.sort(key=lambda x: x.get("date", ""), reverse=True)
    recent_to_display = matches[:10] # Display up to 10 most recent

    lines = []
    for m in recent_to_display:
        sender = m.get("senderUsername") or m.get("senderFirstName") or "User"
        dt_str = m.get("date", "Unknown time")
        try: # Try to format date nicely
            dt_obj = datetime.fromisoformat(dt_str.replace("Z", "+00:00"))
            formatted_date = dt_obj.strftime("%Y-%m-%d %H:%M")
        except ValueError:
            formatted_date = dt_str
        txt = m.get("text", "")
        lines.append(f"[{sender} @ {formatted_date}]: {txt[:150]}{'...' if len(txt)>150 else ''}") # Preview
    
    search_results_header = f"**Found {len(matches)} match(es) for \"{query}\". Showing up to 10 recent:**\n"
    search_results = "\n".join(lines)
    
    # Telegram messages have a length limit (around 4096 chars)
    full_reply = search_results_header + search_results
    if len(full_reply) > 4000:
        full_reply = full_reply[:4000] + "\n... (message truncated)"
        
    await event.reply(full_reply)

async def handle_fuzzy_search_command(entity_id, filename_key, query, event):
    if not query:
        await event.reply("Please provide a query to fuzzy search. Usage: `!fsearch <query>`")
        return

    file_path = os.path.join(LOGS_DIR, filename_key)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map.get(filename_key, [])
    all_msgs = file_msgs + mem_msgs

    scored_matches = []
    for m in all_msgs:
        txt = m.get("text", "")
        if not txt.strip():
            continue
        score = fuzzy_score(txt, query)
        if score > 0.5: # Threshold for considering a match (adjust as needed)
            match_data = m.copy()
            match_data["score"] = score
            scored_matches.append(match_data)

    if not scored_matches:
        await event.reply(f'No significant fuzzy matches found for "{query}".')
        return

    scored_matches.sort(key=lambda x: (x["score"], x.get("date", "")), reverse=True) # Sort by score, then date
    top_matches_to_display = scored_matches[:10]

    lines = []
    for m in top_matches_to_display:
        sender = m.get("senderUsername") or m.get("senderFirstName") or "User"
        dt_str = m.get("date", "Unknown time")
        try:
            dt_obj = datetime.fromisoformat(dt_str.replace("Z", "+00:00"))
            formatted_date = dt_obj.strftime("%Y-%m-%d %H:%M")
        except ValueError:
            formatted_date = dt_str
        txt = m.get("text", "")
        score_percent = m['score'] * 100
        lines.append(f"({score_percent:.0f}%) [{sender} @ {formatted_date}]: {txt[:150]}{'...' if len(txt)>150 else ''}")

    results_header = f"**Top {len(top_matches_to_display)} fuzzy search results for \"{query}\":**\n"
    results_msg = "\n".join(lines)
    
    full_reply = results_header + results_msg
    if len(full_reply) > 4000:
        full_reply = full_reply[:4000] + "\n... (message truncated)"
        
    await event.reply(full_reply)

async def handle_z_command(entity_id, event):
    sender_id = event.sender_id
    if str(sender_id) in UNAUTHORIZED_IDS: # Specific check for this command
        await event.reply("You are not authorized for this specific command.")
        return

    text = event.raw_text
    search_term = text[len(COMMANDS["z"]["prefix"]):].strip()
    if not search_term:
        await event.reply(f"Usage: `{COMMANDS['z']['prefix']} <search-term>`")
        return

    cmd = [ZINDEX_PATH, search_term]
    logger.info(f"Executing /z command: {' '.join(cmd)}")

    unique_lines = set()
    output_buffer = ""
    
    try:
        # Using asyncio.create_subprocess_exec for better async integration
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE
        )

        # Read stdout and stderr concurrently
        # Set a timeout for the subprocess execution
        try:
            stdout_data, stderr_data = await asyncio.wait_for(proc.communicate(), timeout=30.0) # 30s timeout
        except asyncio.TimeoutError:
            logger.warning(f"/z command '{search_term}' timed out. Killing process.")
            proc.kill()
            await proc.wait() # Ensure process is cleaned up
            await event.reply(f"Search for \"{search_term}\" timed out.")
            return

        if proc.returncode != 0 and stderr_data:
            error_message = stderr_data.decode(errors='ignore').strip()
            logger.error(f"/z command error (code {proc.returncode}): {error_message}")
            await event.reply(f"Error running /z command. Return code: {proc.returncode}. Error: {error_message[:1000]}")
            return
        
        if stdout_data:
            output_buffer = stdout_data.decode(errors='ignore')
            # Process output_buffer
            for line in output_buffer.splitlines():
                if line.strip(): # Add non-empty lines
                    unique_lines.add(line.strip())
                if len(unique_lines) >= MAX_UNIQUE_LINES:
                    break
        
    except FileNotFoundError:
        logger.error(f"Error running /z command: {ZINDEX_PATH} not found.")
        await event.reply(f"Error: The search program (`{ZINDEX_PATH}`) was not found.")
        return
    except Exception as e:
        logger.exception(f"Error running /z command with subprocess: {e}")
        await event.reply(f"An unexpected error occurred while running the /z command: {e}")
        return

    if not unique_lines:
        await event.reply(f'No results found for "{search_term}".')
        return

    final_out = "\n".join(list(unique_lines)[:MAX_UNIQUE_LINES]) # Ensure we don't exceed max lines
    results_filename = f"z_results_{slugify(search_term)}.txt"
    
    try:
        # If output is small enough, send directly. Otherwise, send as file.
        if len(final_out) < 3800 : # Leave some room for header
            reply_message = f"{len(unique_lines)} Search results for \"{search_term}\" (up to {MAX_UNIQUE_LINES} unique lines):\n```\n{final_out}\n```"
            await event.reply(reply_message)
        else:
            with open(results_filename, "w", encoding="utf-8") as f:
                f.write(final_out)
            await event.reply(
                f"{len(unique_lines)} Search results for \"{search_term}\" (up to {MAX_UNIQUE_LINES} unique lines) in attached file.",
                file=results_filename
            )
            os.remove(results_filename) # Clean up file after sending
    except Exception as e:
        logger.error(f"Error sending /z search results: {e}")
        await event.reply(f"Error sending search results: {e}")

async def handle_arf_command(entity_id, event):
    for i in range(3):
        await event.reply("ARF!")
        if i < 2: # Don't sleep after the last ARF
            await asyncio.sleep(0.75) # Slightly faster arfing

async def handle_w_command(entity_id, parsed_command_data, event):
    content_parts = parsed_command_data["content"].split()
    if not content_parts:
        await event.reply(f"Usage: `{COMMANDS['w']['prefix']} [+pdf] [+inline] <url>`")
        return

    pdf_flag = False
    inline_flag = False
    content_url = None

    # Flexible flag and URL parsing
    url_candidates = []
    for part in content_parts:
        if part.lower() == "+pdf":
            pdf_flag = True
        elif part.lower() == "+inline":
            inline_flag = True
        elif "://" in part: # Basic check for a URL
            url_candidates.append(part)
        # Could add more sophisticated URL detection if needed

    if not url_candidates:
        await event.reply("No URL provided. Usage: `!w [+pdf] [+inline] <url>`")
        return
    content_url = url_candidates[0] # Take the first detected URL

    if not COMMANDS["w"]["endpoint"]:
        await event.reply("The !w command endpoint is not configured.")
        return

    await event.reply(f"Fetching content from `{content_url}`...")
    response_data = await send_prompt_and_get_response("w", content_url)

    if isinstance(response_data, str): # Error message or simple text response
        await event.reply(response_data)
        return

    if isinstance(response_data, dict) and "data" in response_data and "text" in response_data:
        text_md = response_data["text"]
        filename_slug = response_data["name"]
        binary_md_data = response_data["data"] # This is bytes

        if not text_md.strip():
            await event.reply("Fetched content is empty or contains no usable markdown text.")
            return

        reply_message_header = f"Content from `{content_url}`:"
        files_to_send: List[Tuple[str, bytes]] = []

        if pdf_flag:
            await event.reply("Converting to PDF...")
            pdf_data = convert_markdown_to_pdf(text_md)
            if pdf_data:
                files_to_send.append((f"{filename_slug}.pdf", pdf_data))
            else:
                reply_message_header += "\n(PDF conversion failed or produced empty file)"
        
        if not inline_flag or pdf_flag: # Always send MD file if not inline or if PDF is also requested
             files_to_send.append((f"{filename_slug}.md", binary_md_data))

        if inline_flag:
            # Max length for Telegram message is 4096.
            # Truncate inline text carefully.
            max_inline_len = 3800 # Leave space for header and "..."
            inline_text_display = text_md
            if len(inline_text_display) > max_inline_len:
                inline_text_display = text_md[:max_inline_len] + "\n... (truncated)"
            await event.reply(f"{reply_message_header}\n\n{inline_text_display}", file=files_to_send if files_to_send else None)
        elif files_to_send:
            await event.reply(reply_message_header, file=files_to_send)
        else: # No files to send (e.g. PDF failed, not inline) and no inline text
            await event.reply(f"{reply_message_header}\nNo files were generated (e.g., PDF conversion failed and not sending inline/MD).")
            
    else:
        logger.error(f"Unexpected response type from !w command processing: {type(response_data)}")
        await event.reply("Unexpected response or error from the !w command's content service.")

async def handle_q_and_b_command(entity_id, parsed_command_data, event):
    command_key = parsed_command_data["command"]
    content_query = parsed_command_data["content"]
    command_config = COMMANDS[command_key]

    if not command_config.get("endpoint"):
        await event.reply(f"The `{command_config['prefix']}` command endpoint is not configured.")
        return
    if command_key == "q" and not LLMM1: # LLMM1 specific to !q in current setup
        await event.reply(f"The model for `{command_config['prefix']}` (LLMM1) is not configured in .env.")
        return

    llm_context_text = ""
    if command_key == "b": # Only use LLM_CONTEXT_PATH for !cb
        if LLM_CONTEXT_PATH and os.path.exists(LLM_CONTEXT_PATH):
            try:
                with open(LLM_CONTEXT_PATH, "r", encoding="utf-8") as f:
                    llm_context_text = f.read().strip()
                if not llm_context_text:
                    logger.info(f"LLM context file {LLM_CONTEXT_PATH} is empty.")
            except Exception as e:
                logger.error(f"Error reading LLM context file ({LLM_CONTEXT_PATH}): {e}")
                await event.reply(f"Error reading context file for `{command_config['prefix']}`.")
                # Decide if you want to proceed without context or stop
                # return
        elif LLM_CONTEXT_PATH:
            logger.warning(f"LLM context file {LLM_CONTEXT_PATH} not found for command {command_key}.")
            # await event.reply(f"Context file not found for `{command_config['prefix']}`. Proceeding without it.")


    await event.reply(f"Processing `{command_config['prefix']}` request...")
    response_text = await send_prompt_and_get_response(command_key, content_query, llm_context_text)
    
    # Check if response_text indicates an error (send_prompt already formats some errors)
    if response_text.startswith("Error:") or response_text.startswith("Sorry,"):
        await event.reply(f"{command_config['prefix']} -> {response_text}")
    else:
        # Telegram message length limit
        if len(response_text) > 4000:
            response_text = response_text[:4000] + "\n... (response truncated)"
        await event.reply(f"{command_config['prefix']} {content_query}\n\n{response_text}")


# ----------------------------------------------------------------------
# Main Event Handler
# ----------------------------------------------------------------------
@client.on(events.NewMessage(incoming=True, forwards=False))
async def handle_new_message(event):
    msg_text_raw = event.raw_text or ""
    message_object = event.message
    sender_id = event.sender_id

    # --- Determine entity ID and name for logging/cache ---
    peer_id_obj = message_object.peer_id
    entity_id_val: Optional[int] = None
    if hasattr(peer_id_obj, "channel_id"):
        entity_id_val = peer_id_obj.channel_id
    elif hasattr(peer_id_obj, "user_id"):
        entity_id_val = peer_id_obj.user_id
    elif hasattr(peer_id_obj, "chat_id"):
        entity_id_val = peer_id_obj.chat_id

    if entity_id_val is None: # Should not happen for normal messages
        logger.warning(f"Could not determine entity_id for message: {message_object.id}")
        return

    entity_name_or_id_str: str
    if entity_id_val not in entity_cache:
        try:
            entity_obj = await client.get_entity(peer_id_obj)
            # Prefer title (channels/groups), then username, then first_name
            if hasattr(entity_obj, "title") and entity_obj.title:
                entity_name_or_id_str = entity_obj.title
            elif hasattr(entity_obj, "username") and entity_obj.username:
                entity_name_or_id_str = entity_obj.username
            elif hasattr(entity_obj, "first_name") and entity_obj.first_name:
                entity_name_or_id_str = entity_obj.first_name
            else:
                entity_name_or_id_str = str(entity_id_val)
            entity_cache[entity_id_val] = slugify(entity_name_or_id_str) # Cache slugified version
        except Exception as e:
            logger.error(f"Entity fetch error for ID {entity_id_val}: {e}")
            entity_cache[entity_id_val] = str(entity_id_val) # Cache the ID itself as string
    
    filename_slug_prefix = entity_cache[entity_id_val]

    # --- Prepare for logging ---
    os.makedirs(LOGS_DIR, exist_ok=True)
    current_date_str = datetime.utcnow().strftime("%Y-%m-%d")
    # This is the key for messages_map and the filename on disk
    log_file_key = f"{filename_slug_prefix}-{current_date_str}.json"

    timestamp_obj = message_object.date # This is already a datetime object
    datetime_iso_str = timestamp_obj.isoformat()

    message_data_to_log = {
        "messageId": message_object.id,
        "date": datetime_iso_str,
        "text": msg_text_raw,
        "senderId": sender_id,
        "senderUsername": getattr(message_object.sender, "username", None),
        "senderFirstName": getattr(message_object.sender, "first_name", None),
        "senderLastName": getattr(message_object.sender, "last_name", None)
    }
    messages_map[log_file_key].append(message_data_to_log)

    # --- Command Parsing & Handling ---
    parsed_cmd = parse_command(msg_text_raw)

    # Reply-chaining: if no direct command, check if replying to a message from MY_USERNAME
    if not parsed_cmd and message_object.reply_to_msg_id:
        try:
            replied_to_msg = await event.get_reply_message()
            if replied_to_msg and replied_to_msg.sender and getattr(replied_to_msg.sender, 'username', None) == MY_USERNAME:
                # Check if the bot's message itself was a command response
                original_bot_response_text = replied_to_msg.raw_text or ""
                # Attempt to parse command from the *bot's* message
                potential_original_cmd = parse_command(original_bot_response_text)
                if potential_original_cmd:
                    # Use the current user's message as new content for the original command
                    potential_original_cmd["content"] = msg_text_raw # User's new message is the content
                    parsed_cmd = potential_original_cmd
                    logger.info(f"Reply-chaining activated for command '{parsed_cmd['command']}'")
        except Exception as e:
            logger.warning(f"Error during reply-chaining check: {e}")


    if not parsed_cmd:
        return # Not a command, nothing more to do

    # Authorization check
    if not is_user_authorized(sender_id):
        logger.warning(f"Unauthorized attempt by user {sender_id} ({getattr(event.sender, 'username', 'N/A')}) for command: {msg_text_raw}")
        await event.reply("You are not authorized to use this bot.")
        return

    logger.info(f"Authorized user {sender_id} ({getattr(event.sender, 'username', 'N/A')}) "
                f"ran command: {parsed_cmd['command']} with content: \"{parsed_cmd['content'][:50]}...\"")

    cmd_type = parsed_cmd["command"]

    # --- Dispatch to command handlers ---
    try:
        if cmd_type == "help":
            await handle_help_command(event)
        elif cmd_type == "summarize":
            await handle_summarize_command(entity_id_val, filename_slug_prefix, event)
        elif cmd_type == "stats":
            await handle_stats_command(entity_id_val, log_file_key, event)
        elif cmd_type == "search":
            await handle_search_command(entity_id_val, log_file_key, parsed_cmd["content"], event)
        elif cmd_type == "fsearch":
            await handle_fuzzy_search_command(entity_id_val, log_file_key, parsed_cmd["content"], event)
        elif cmd_type == "z":
            await handle_z_command(entity_id_val, event) # event contains raw_text
        elif cmd_type == "w":
            await handle_w_command(entity_id_val, parsed_cmd, event)
        elif cmd_type == "arf":
            await handle_arf_command(entity_id_val, event)
        elif cmd_type in ["q", "b"]:
            await handle_q_and_b_command(entity_id_val, parsed_cmd, event)
        else:
            logger.warning(f"Unknown command type dispatched: {cmd_type}")
            # await event.reply(f"Sorry, I don't know the command '{cmd_type}'. Try `!help`.") # Optional
    except Exception as e:
        logger.exception(f"Error handling command '{cmd_type}': {e}")
        await event.reply(f"An internal error occurred while processing your command ({cmd_type}). Please try again later or contact the admin.")


# ----------------------------------------------------------------------
# Login & Main
# ----------------------------------------------------------------------
async def main_bot_loop():
    """Connects the client and runs until disconnected."""
    login_attempts = 0
    while login_attempts < MAX_LOGIN_ATTEMPTS:
        try:
            logger.info(f"Attempting Telegram login (attempt {login_attempts + 1}/{MAX_LOGIN_ATTEMPTS})...")
            # Start client. Lambda for phone, input for code, and now for password.
            await client.start(
                phone=lambda: PHONE_NUMBER,
                code_callback=lambda: input('Enter the Telegram code you received: '),
                password=lambda: input('Your Telegram account has 2FA enabled. Please enter your password: ') # ENHANCEMENT
            )
            # A second check for authorization status, client.is_connected() might be enough after start()
            if not await client.is_user_authorized(): # This check might be redundant if start() succeeds
                logger.error("Telegram login failed: User not authorized after start. This state should not be reached if start() is successful.")
                # This part of logic might need review based on Telethon's exact behavior of start() with 2FA.
                # If start() handles 2FA and succeeds, client.is_user_authorized() should be true.
                # If it fails, an exception (like SessionPasswordNeededError handled by password lambda) occurs.
                # Forcing a sign_in call here might be problematic if start() already managed session.
                # Consider removing this explicit sign_in block if start() with password lambda is robust.
                #
                # logger.info("Attempting explicit sign-in...")
                # await client.sign_in(phone=PHONE_NUMBER, code=input('Enter code: '), password=input('Enter 2FA password: '))
                #
                # The `password` lambda in `client.start()` should handle the SessionPasswordNeededError case.
                # If `start()` completes without error, we should be authorized.
            logger.info("Telegram login successful!")
            break
        except SessionPasswordNeededError: # Should be caught by the password lambda in client.start now
            logger.error("Telegram login failed: SessionPasswordNeededError was not handled by start's password callback. This is unexpected.")
            # This indicates an issue with how Telethon's start() calls the password lambda or if it's bypassed.
            # For robustness, we can offer another chance here, but ideally, start() handles it.
            try:
                password = input("Your Telegram account has 2FA enabled. Please enter your password: ")
                await client.sign_in(password=password)
                logger.info("Telegram login successful after manual 2FA password entry!")
                break
            except Exception as e_pw:
                login_attempts += 1
                logger.error(f"Login attempt {login_attempts} failed during manual 2FA: {e_pw}")
                if login_attempts >= MAX_LOGIN_ATTEMPTS: break
                time.sleep(5) # Wait before retrying
        except Exception as e:
            login_attempts += 1
            logger.error(f"Telegram login attempt {login_attempts} failed: {e}")
            if "PHONE_CODE_INVALID" in str(e):
                logger.warning("Invalid phone code entered.")
            elif "FLOOD_WAIT" in str(e):
                match = re.search(r"FLOOD_WAIT_(\d+)", str(e))
                wait_secs = int(match.group(1)) if match else 30 # Default wait if regex fails
                logger.info(f"Telegram API flood wait: Need to wait {wait_secs} seconds before trying again.")
                time.sleep(wait_secs)
            else:
                time.sleep(min(10, login_attempts * 2)) # Exponential backoff for other errors

    if not await client.is_connected() or not await client.is_user_authorized():
        logger.critical("Max Telegram login attempts reached or login failed. Exiting.")
        return

    # Create background task to save logs
    asyncio.create_task(save_messages_to_file())

    logger.info("Bot is running. Press Ctrl+C to stop.")
    await client.run_until_disconnected()


if __name__ == "__main__":
    if not all([TG_API_ID, TG_API_HASH, PHONE_NUMBER, MY_USERNAME]):
         logger.critical("Core Telegram credentials (TG_API_ID, TG_API_HASH, PHONE_NUMBER, MY_USERNAME) are not set in .env. Exiting.")
    else:
        try:
            asyncio.run(main_bot_loop())
        except KeyboardInterrupt:
            logger.info("Bot shutting down via KeyboardInterrupt.")
        except Exception as e_global:
            logger.critical(f"Global unhandled exception in main loop: {e_global}", exc_info=True)
        finally:
            # Ensure client disconnects if loop exited abruptly
            if client.is_connected():
                logger.info("Ensuring client is disconnected...")
                # asyncio.run(client.disconnect()) # This can cause issues if event loop is closed
                # It's generally better Telethon handles its own cleanup on run_until_disconnected exit.
            logger.info("Bot has stopped.")
