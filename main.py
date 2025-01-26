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
from datetime import datetime, timedelta
from collections import defaultdict

import requests
import openai
from telethon import TelegramClient, events
from telethon.errors import SessionPasswordNeededError
from dotenv import load_dotenv


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

AUTHORIZED_IDS = [
    "<REDACTED>"
]
UNAUTHORIZED_IDS = [
    "<REDACTED>"
]

REQUIRED_ENV = [
    TG_API_ID, TG_API_HASH, PHONE_NUMBER, OPENAI_API_KEY,
    MY_USERNAME, LLM_CONTEXT_PATH, COMMAND_BASE_URL
]
if any(v is None for v in REQUIRED_ENV):
    print("Missing required environment variables. Check your .env file.")
    exit(1)


# ----------------------------------------------------------------------
# Constants & Initialization
# ----------------------------------------------------------------------
LOGS_DIR = "./logs"           
MAX_UNIQUE_LINES = 500        
MAX_LOGIN_ATTEMPTS = 3        
SAVE_INTERVAL_SECONDS = 5 * 60  

messages_map = defaultdict(list)
entity_cache = {}

# Initialize Telethon client
client = TelegramClient(SESSION_NAME, int(TG_API_ID), TG_API_HASH)

# Configure OpenAI
openai.api_key = OPENAI_API_KEY

COMMANDS = {
    "q": {
        "prefix": "!q",
        "endpoint": f"{COMMAND_BASE_URL}/q",
        "params": {
            "temperature": 0.3,
            "tokens": 3000,
            "model": LLMM1,
            "version": "v2",
        },
    },
    "b": {
        "prefix": "!cb",
        "endpoint": f"{COMMAND_BASE_URL}/q",
        "params": {
            "temperature": 0.92,
            "tokens": 4000,
            "format": "fun",
            "model": "openai-next",
            "version": "v2",
        },
    },
    "w": {
        "prefix": "!w",
        "endpoint": OZZO_GETCONT,
        "params": {
            "headers": {
                "X-API-KEY": OZZO_API_KEY,
                "Content-Type": "application/json",
            },
        },
    },
    "arf": {
        "prefix": "!arf"
    },
    "summarize": {
        "prefix": "!cai"
    },
    "search": {
        "prefix": "!search"
    },
    "stats": {
        "prefix": "!stats"
    },
    "fsearch": {
        "prefix": "!fsearch"
    },
    "z": {
        "prefix": "/z"
    },
}


# ----------------------------------------------------------------------
# Utility Functions
# ----------------------------------------------------------------------
def is_user_authorized(user_id: int) -> bool:
    """Checks if a given user ID is in the authorized list."""
    return str(user_id) in AUTHORIZED_IDS

def slugify(text: str) -> str:
    """Slugify a text string (lowercase, remove special chars, etc.)."""
    import unicodedata
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

def parse_command(text: str):
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
async def send_prompt_and_get_response(command: str, content: str, context: str = ""):
    """
    Posts content+context to the configured LLM endpoint.
    Returns either a string or an object with file data.
    Now includes robust error handling and fallback responses.
    """
    cmd_config = COMMANDS.get(command, {})
    endpoint = cmd_config.get("endpoint", "")
    params = cmd_config.get("params", {})
    if not endpoint:
        return "Error: No endpoint configured for this command."

    try:
        if command == "w":
            # OZZO content fetch
            headers = params.get("headers", {})
            payload = {"url": content}
            try:
                resp = requests.post(endpoint, json=payload, headers=headers, timeout=15)
                resp.raise_for_status()
            except requests.exceptions.HTTPError as e:
                return f"Sorry, there was an HTTP error fetching content: {e}"
            except requests.exceptions.RequestException as e:
                return f"Network error fetching content: {e}"

            try:
                data = resp.json()
                text_md = data.get("markdown", "")
                if not text_md:
                    return "No markdown data found in response."
                return {
                    "name": slugify(content),
                    "data": text_md.encode("utf-8"),
                    "text": text_md
                }
            except (ValueError, KeyError) as e:
                return f"Error parsing response JSON: {e}"

        # Otherwise, generic LLM endpoint
        payload = {
            "question": content,
            "context": context,
            **params
        }
        try:
            resp = requests.post(endpoint, json=payload, headers={"Content-Type": "application/json"}, timeout=20)
            resp.raise_for_status()
        except requests.exceptions.HTTPError as e:
            return f"HTTP error from AI service: {e}"
        except requests.exceptions.RequestException as e:
            return f"Network error from AI service: {e}"

        try:
            data = resp.json()
        except ValueError:
            return "Error: Failed to parse AI response (invalid JSON)."
        # Example: .assistant might have the text
        assistant_msg = data.get("assistant", "")
        if not assistant_msg:
            return "AI returned an empty response."
        return assistant_msg

    except Exception as e:
        return f"Sorry, an unexpected error occurred: {e}"

def convert_markdown_to_pdf(markdown: str) -> bytes:
    """
    Converts markdown text into PDF (calling a remote service).
    If the service is not available or returns invalid data, returns empty bytes.
    """
    params = {
        "markdown": markdown
    }
    try:
        r = requests.post("https://md-to-pdf.fly.dev", data=params, timeout=15)
        r.raise_for_status()
        return r.content
    except requests.exceptions.RequestException as e:
        print(f"[Error] convert_markdown_to_pdf: {e}")
        return b""
    except Exception as e:
        print(f"[Error] convert_markdown_to_pdf (other): {e}")
        return b""


# ----------------------------------------------------------------------
# Summaries with OpenAI or "Uncensored" Endpoint, with Error Handling
# ----------------------------------------------------------------------
async def get_chat_summary(chat_text: str) -> str:
    """
    Summarize chat text using official OpenAI GPT endpoints.
    In case of error or empty response, fallback gracefully.
    """
    if not chat_text.strip():
        return "No chat data to summarize."

    try:
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
            return "OpenAI returned no summary."
        return content
    except openai.error.OpenAIError as e:
        return f"OpenAI Error: {e}"
    except Exception as e:
        return f"Error summarizing chat: {e}"

async def get_chat_summary_uncensored(chat_text: str) -> str:
    """
    Summarize chat with an alternative local or uncensored model endpoint.
    Returns fallback message if service is unavailable or data is invalid.
    """
    if not chat_text.strip():
        return "No chat data to summarize."

    payload = {
        "model": "solar-10.7b-instruct-v1.0-uncensored",
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
        r = requests.post(f"{UNCENSORED_AI_URL}/v1/chat/completions",
                          json=payload, timeout=20)
        r.raise_for_status()
        data = r.json()
        choices = data.get("choices", [])
        if not choices:
            return "No summary returned by uncensored model."
        content = choices[0]["message"].get("content", "")
        return content if content.strip() else "Empty content from uncensored model."
    except requests.exceptions.HTTPError as e:
        return f"Uncensored model HTTP error: {e}"
    except requests.exceptions.RequestException as e:
        return f"Uncensored model network error: {e}"
    except (ValueError, KeyError, IndexError) as e:
        return f"Uncensored model response parse error: {e}"
    except Exception as e:
        return f"Unhandled error from uncensored model: {e}"


# ----------------------------------------------------------------------
# Logging Helpers
# ----------------------------------------------------------------------
def read_log_file(filepath: str):
    """Reads a JSON file of messages, returns list or empty."""
    if not os.path.exists(filepath):
        return []
    try:
        with open(filepath, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception as e:
        print(f"[Error reading {filepath}]: {e}")
        return []

def write_log_file(filepath: str, messages: list):
    """Writes messages list as JSON."""
    data_str = json.dumps(messages, ensure_ascii=False, indent=2)
    try:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(data_str)
    except Exception as e:
        print(f"[Error writing {filepath}]: {e}")

async def save_messages_to_file():
    """Periodically saves unsaved messages from memory to disk."""
    while True:
        await asyncio.sleep(SAVE_INTERVAL_SECONDS)
        for filename, unsaved in messages_map.items():
            if unsaved:
                filepath = os.path.join(LOGS_DIR, filename)
                existing = read_log_file(filepath)
                merged = existing + unsaved
                write_log_file(filepath, merged)
                messages_map[filename].clear()


# ----------------------------------------------------------------------
# Command Handlers (With Some Extra Error Handling)
# ----------------------------------------------------------------------
async def handle_summarize_command(entity_id, entity_name_or_id, event):
    msg_text = event.raw_text
    args = msg_text.split()
    days_ago = 0

    if "-d" in args:
        idx = args.index("-d")
        if idx + 1 < len(args):
            try:
                days_ago = int(args[idx + 1])
            except:
                days_ago = 0

    target_date = datetime.utcnow() - timedelta(days=days_ago)
    date_string = target_date.strftime("%Y-%m-%d")
    filename_for_sum = f"{entity_name_or_id}-{date_string}.json"

    file_path = os.path.join(LOGS_DIR, filename_for_sum)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map[filename_for_sum]
    combined = file_msgs + mem_msgs

    if not combined:
        await event.reply("No messages found to summarize.")
        return

    recent_msgs = combined[-200:]
    chat_text = "\n".join(
        f"{m.get('senderUsername') or m.get('senderFirstName') or 'User'}: {m.get('text')}"
        for m in recent_msgs
    )

    summary = await get_chat_summary_uncensored(chat_text)
    await event.reply(f"**Summary ({days_ago} day(s) ago):**\n{summary}")

async def handle_stats_command(entity_id, filename, event):
    file_path = os.path.join(LOGS_DIR, filename)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map[filename]
    all_msgs = file_msgs + mem_msgs

    if not all_msgs:
        await event.reply("No messages logged yet for today.")
        return

    # Basic stats
    total_messages = len(all_msgs)
    participants = set()
    for m in all_msgs:
        name = m.get("senderUsername") or m.get("senderFirstName") or "User"
        participants.add(name)
    unique_participants = len(participants)

    user_count = {}
    for m in all_msgs:
        name = m.get("senderUsername") or m.get("senderFirstName") or "User"
        user_count[name] = user_count.get(name, 0) + 1
    sorted_users = sorted(user_count.items(), key=lambda x: x[1], reverse=True)
    top_users_str = "\n".join(f"{u}: {c} messages" for u, c in sorted_users[:5])

    total_len = sum(len(m.get("text", "")) for m in all_msgs)
    avg_len = round(total_len / total_messages, 2) if total_messages else 0

    word_freq = {}
    for m in all_msgs:
        txt = m.get("text", "")
        words = re.split(r"[\s,!.?;:()]+", txt.lower())
        for w in words:
            if w:
                word_freq[w] = word_freq.get(w, 0) + 1
    top_word_entries = sorted(word_freq.items(), key=lambda x: x[1], reverse=True)[:10]
    top_words_str = ", ".join([f"{w} ({c})" for w, c in top_word_entries]) if top_word_entries else "N/A"

    hours = [0]*24
    for m in all_msgs:
        d_str = m.get("date")
        if d_str:
            try:
                d = datetime.fromisoformat(d_str.replace("Z", ""))
                hours[d.hour] += 1
            except:
                pass
    hour_list = [{"hour": i, "count": c} for i, c in enumerate(hours)]
    hour_list.sort(key=lambda x: x["count"], reverse=True)
    top_hours_str = "\n".join(f"{h['hour']}:00 - {h['count']} msg" for h in hour_list[:3]) if hour_list else "N/A"

    cmd_counts = {}
    for m in all_msgs:
        t = m.get("text", "").strip()
        if t.startswith("!") or t.startswith("/"):
            cmd = t.split()[0]
            cmd_counts[cmd] = cmd_counts.get(cmd, 0) + 1
    sorted_cmds = sorted(cmd_counts.items(), key=lambda x: x[1], reverse=True)[:5]
    top_cmds_str = ", ".join([f"{cmd} ({count})" for cmd, count in sorted_cmds]) if sorted_cmds else "N/A"

    stats_msg = (
        f"**Today's Stats**:\n"
        f"• Total messages: `{total_messages}`\n"
        f"• Unique participants: `{unique_participants}`\n"
        f"• Top participants:\n{top_users_str or 'No messages yet.'}\n\n"
        f"• Avg message length: `{avg_len}` chars\n"
        f"• Top 10 words: {top_words_str}\n"
        f"• Top 3 hours:\n{top_hours_str}\n\n"
        f"• Top 5 commands: {top_cmds_str}"
    )
    await event.reply(stats_msg)

async def handle_search_command(entity_id, filename, query, event):
    if not query:
        await event.reply("Please provide a search keyword.")
        return

    file_path = os.path.join(LOGS_DIR, filename)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map[filename]
    all_msgs = file_msgs + mem_msgs

    matches = [m for m in all_msgs if m.get("text") and query.lower() in m.get("text").lower()]
    recent = matches[-10:]
    if not recent:
        await event.reply(f'No matches found for "{query}".')
        return

    lines = []
    for m in recent:
        sender = m.get("senderUsername") or m.get("senderFirstName") or "User"
        dt = m.get("date")
        txt = m.get("text")
        lines.append(f"[{sender} at {dt}]: {txt}")
    search_results = "\n".join(lines)
    await event.reply(f"**Search results for \"{query}\":**\n{search_results}")

async def handle_fuzzy_search_command(entity_id, filename, query, event):
    if not query:
        await event.reply("Please provide a query to fuzzy search.")
        return

    file_path = os.path.join(LOGS_DIR, filename)
    file_msgs = read_log_file(file_path)
    mem_msgs = messages_map[filename]
    all_msgs = file_msgs + mem_msgs

    scored = []
    for m in all_msgs:
        txt = m.get("text", "")
        if not txt.strip():
            continue
        score = fuzzy_score(txt, query)
        if score > 0:
            new_m = m.copy()
            new_m["score"] = score
            scored.append(new_m)

    scored.sort(key=lambda x: x["score"], reverse=True)
    top_matches = scored[:10]
    if not top_matches:
        await event.reply(f'No fuzzy matches found for "{query}".')
        return

    lines = []
    for m in top_matches:
        sender = m.get("senderUsername") or m.get("senderFirstName") or "User"
        dt = m.get("date")
        txt = m.get("text")
        lines.append(f"[{sender} at {dt}]: {txt}")
    results_msg = "\n".join(lines)
    await event.reply(f"**Fuzzy search results for \"{query}\":**\n{results_msg}")

async def handle_z_command(entity_id, event):
    # /z command
    sender_id = event.sender_id
    if str(sender_id) in UNAUTHORIZED_IDS:
        await event.reply("Nah.")
        return

    text = event.raw_text
    search_term = text[len("/z"):].strip()
    if not search_term:
        await event.reply("Usage: /z <search-term>")
        return

    cmd = ["./zindex5", search_term]  # Adjust path as needed

    unique_lines = set()
    leftover = ""
    done = False

    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    except Exception as e:
        await event.reply(f"Error running /z command: {e}")
        return

    while True:
        line = proc.stdout.readline()
        if not line:
            if proc.poll() is not None:
                break
            continue

        if done:
            continue

        # Very naive logic to parse lines
        # The JS version did some parsing around '"preview"'. You can adapt as needed.
        # ...
        if line.strip():
            unique_lines.add(line.strip())
            if len(unique_lines) >= MAX_UNIQUE_LINES:
                done = True
                proc.kill()

    proc.stdout.close()
    proc.stderr.close()

    if not unique_lines:
        await event.reply(f'No results for "{search_term}".')
        return

    final_out = "\n".join(unique_lines)
    results_filename = "search_results.txt"
    try:
        with open(results_filename, "w", encoding="utf-8") as f:
            f.write(final_out)
        await event.reply(
            f"{len(unique_lines)} Search results for \"{search_term}\" (up to {MAX_UNIQUE_LINES} unique lines).",
            file=results_filename
        )
    except Exception as e:
        await event.reply(f"Error sending search results: {e}")

async def handle_arf_command(entity_id, event):
    for _ in range(3):
        await event.reply("ARF!")
        await asyncio.sleep(1)

async def handle_w_command(entity_id, parsed_command, event):
    """!w command with robust error handling for missing or malformed data."""
    parts = parsed_command["content"].split()
    if not parts:
        await event.reply("Usage: !w [+pdf] [+inline] <url>")
        return

    pdf_flag = False
    inline_flag = False
    content_url = None

    if len(parts) == 1:
        content_url = parts[0]
    else:
        flags = parts[0].lower()
        if "+pdf" in flags:
            pdf_flag = True
        if "+inline" in flags:
            inline_flag = True
        if len(parts) > 1:
            content_url = parts[1]

    if not content_url:
        await event.reply("No URL provided for !w command.")
        return

    resp = await send_prompt_and_get_response("w", content_url)

    # If it's an error or string
    if isinstance(resp, str):
        # Could be an error message or plain text
        if resp.startswith("Sorry,") or "error" in resp.lower():
            await event.reply(resp)
        else:
            # Just a normal text from the endpoint
            await event.reply(resp)
        return

    if isinstance(resp, dict) and "data" in resp and "text" in resp:
        text_md = resp["text"]
        filename_slug = resp["name"]

        if not text_md.strip():
            await event.reply("No usable markdown text from the w command.")
            return

        if pdf_flag and inline_flag:
            pdf_data = convert_markdown_to_pdf(text_md)
            await event.reply(
                f"Content for {content_url}:\n\n{text_md[:2000]}",
                file=[("converted.pdf", pdf_data)] if pdf_data else None
            )
        elif pdf_flag:
            pdf_data = convert_markdown_to_pdf(text_md)
            md_data = resp["data"]
            files_to_send = []
            if md_data:
                files_to_send.append((f"{filename_slug}.md", md_data))
            if pdf_data:
                files_to_send.append((f"{filename_slug}.pdf", pdf_data))

            if not files_to_send:
                await event.reply("No files could be generated.")
                return

            await event.reply(
                f"```Markdown and PDF for ->> {content_url}```",
                file=files_to_send
            )
        elif inline_flag:
            short_text = text_md[:2000]
            await event.reply(f"Content for {content_url}:\n\n{short_text}")
        else:
            md_data = resp["data"]
            if not md_data:
                await event.reply("No .md content to send.")
                return
            await event.reply(
                f"```Markdown for ->> {content_url}```",
                file=[(f"{filename_slug}.md", md_data)]
            )
    else:
        # Catch-all fallback
        await event.reply("Unexpected response type from !w command.")

async def handle_q_and_b_command(entity_id, parsed_command, event):
    command = parsed_command["command"]
    content = parsed_command["content"]

    # If the LLM context file is missing or empty, let's handle that gracefully
    llm_context = ""
    if LLM_CONTEXT_PATH and os.path.exists(LLM_CONTEXT_PATH):
        try:
            with open(LLM_CONTEXT_PATH, "r", encoding="utf-8") as f:
                llm_context = f.read()
        except Exception as e:
            print(f"[Error reading LLM context file]: {e}")

    # Only use context for "!cb"
    use_context = llm_context if command == "b" else ""

    resp = await send_prompt_and_get_response(command, content, use_context)
    prefix = COMMANDS[command]["prefix"]

    if "error" in resp.lower() or resp.startswith("Sorry,"):
        await event.reply(f"{prefix} Error: {resp}")
    else:
        # Normal response
        await event.reply(f"{prefix} {resp}")


# ----------------------------------------------------------------------
# Main Event Handler
# ----------------------------------------------------------------------
@client.on(events.NewMessage(incoming=True, forwards=False))
async def handle_new_message(event):
    msg_text = event.raw_text or ""
    msg = event.message
    sender_id = event.sender_id

    peer = event.message.peer_id
    entity_id = None

    if hasattr(peer, "channel_id"):
        entity_id = peer.channel_id
    elif hasattr(peer, "user_id"):
        entity_id = peer.user_id
    elif hasattr(peer, "chat_id"):
        entity_id = peer.chat_id

    # Resolve or cache entity name
    if entity_id not in entity_cache:
        try:
            entity_obj = await client.get_entity(peer)
            if hasattr(entity_obj, "title"):
                entity_name_or_id = entity_obj.title
            elif hasattr(entity_obj, "username"):
                entity_name_or_id = entity_obj.username
            elif hasattr(entity_obj, "first_name"):
                entity_name_or_id = entity_obj.first_name
            else:
                entity_name_or_id = str(entity_id)
            entity_cache[entity_id] = entity_name_or_id
        except Exception as e:
            print(f"[Entity Fetch Error] {e}")
            entity_name_or_id = str(entity_id)
            entity_cache[entity_id] = entity_name_or_id
    else:
        entity_name_or_id = entity_cache[entity_id]

    os.makedirs(LOGS_DIR, exist_ok=True)
    current_date = datetime.utcnow().strftime("%Y-%m-%d")
    filename = f"{entity_name_or_id}-{current_date}.json"

    ts = msg.date
    dt_str = ts.isoformat() if isinstance(ts, datetime) else datetime.utcnow().isoformat()

    message_data = {
        "messageId": msg.id,
        "date": dt_str,
        "text": msg_text,
        "senderId": sender_id,
        "senderUsername": getattr(msg.sender, "username", None),
        "senderFirstName": getattr(msg.sender, "first_name", None),
        "senderLastName": getattr(msg.sender, "last_name", None)
    }
    messages_map[filename].append(message_data)

    parsed_command = parse_command(msg_text)

    # Reply-chaining: if no direct command, check if replying to a message from me
    if not parsed_command and msg.reply_to_msg_id:
        try:
            replied_msg = await event.get_reply_message()
            if replied_msg and replied_msg.sender:
                if replied_msg.sender.username == MY_USERNAME:
                    fallback_cmd = parse_command(replied_msg.raw_text or "")
                    if fallback_cmd:
                        fallback_cmd["content"] = msg_text
                        parsed_command = fallback_cmd
        except:
            pass

    if not parsed_command:
        return

    # Auth check
    if not is_user_authorized(sender_id):
        await event.reply("You are not authorized to use this bot.")
        return

    ctype = parsed_command["command"]
    if ctype == "summarize":
        await handle_summarize_command(entity_id, entity_name_or_id, event)
    elif ctype == "stats":
        await handle_stats_command(entity_id, filename, event)
    elif ctype == "search":
        await handle_search_command(entity_id, filename, parsed_command["content"], event)
    elif ctype == "fsearch":
        await handle_fuzzy_search_command(entity_id, filename, parsed_command["content"], event)
    elif ctype == "z":
        await handle_z_command(entity_id, event)
    elif ctype == "w":
        await handle_w_command(entity_id, parsed_command, event)
    elif ctype == "arf":
        await handle_arf_command(entity_id, event)
    elif ctype in ["q", "b"]:
        await handle_q_and_b_command(entity_id, parsed_command, event)


# ----------------------------------------------------------------------
# Login & Main
# ----------------------------------------------------------------------
async def main():
    attempts = 0
    while attempts < MAX_LOGIN_ATTEMPTS:
        try:
            await client.start(phone=lambda: PHONE_NUMBER)
            if await client.is_user_authorized() is False:
                try:
                    await client.sign_in(PHONE_NUMBER)
                except SessionPasswordNeededError:
                    raise RuntimeError("2FA password needed. Update code to handle this.")
            print("Login successful!")
            break
        except Exception as e:
            attempts += 1
            print(f"Login attempt {attempts} failed: {e}")
            if "PHONE_CODE_INVALID" in str(e):
                print("Invalid phone code.")
            elif "FLOOD_WAIT" in str(e):
                match = re.search(r"FLOOD_WAIT_(\d+)", str(e))
                if match:
                    wait_secs = int(match.group(1))
                    print(f"Need to wait {wait_secs} seconds before trying again.")
                    time.sleep(wait_secs)
            else:
                time.sleep(2)

    if attempts >= MAX_LOGIN_ATTEMPTS:
        print("Max login attempts reached. Exiting.")
        return

    # Create background task to save logs
    asyncio.create_task(save_messages_to_file())

    print("Bot is running. Press Ctrl+C to stop.")
    await client.run_until_disconnected()


if __name__ == "__main__":
    asyncio.run(main())
