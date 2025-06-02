Okay, this is a feature-rich Telegram bot! Here's a comprehensive README based on the provided Python script.

```markdown
# Telegram Autobot (Python) with Enhanced Error Handling 🤖💬

A versatile Telegram bot built with Python and Telethon, designed for interacting with Large Language Models (LLMs), fetching web content, performing chat analysis, and more. It features robust error handling, configurable commands, and message logging.

## ✨ Features

*   **Telegram Integration:** Uses Telethon for seamless Telegram API interaction.
*   **Multiple LLM Integrations:**
    *   Connects to custom LLM endpoints (e.g., for `!q`, `!cb` commands).
    *   Integrates with OpenAI's GPT models for chat summarization (`!cai`).
    *   Supports an "uncensored" local/alternative LLM endpoint for summarization.
*   **Web Content Fetching (`!w`):**
    *   Fetches content from URLs.
    *   Converts fetched markdown to PDF using an external service.
    *   Option to send content as a file or inline.
*   **Chat Interaction & Analysis:**
    *   **Summarization (`!cai`):** Generates summaries of recent chat messages (configurable for N days ago).
    *   **Statistics (`!stats`):** Provides daily chat statistics (total messages, participants, top users, top words, activity by hour, top commands).
    *   **Message Search (`!search`, `!fsearch`):**
        *   Exact keyword search in logged messages.
        *   Fuzzy search for approximate matches.
    *   **External Index Search (`/z`):** Integrates with an external `zindex5` script/executable for searching an indexed dataset.
*   **Message Logging:**
    *   Logs incoming messages to JSON files, organized by chat and date.
    *   Periodically saves in-memory messages to disk.
*   **Command System:**
    *   Parses messages for predefined command prefixes.
    *   Supports reply-chaining: if you reply to the bot's previous command output with new text, it treats the new text as input for the original command.
*   **Authorization:**
    *   Restricts bot usage to authorized Telegram user IDs.
    *   Can specifically deny access to certain user IDs for specific commands (e.g., `/z`).
*   **Enhanced Error Handling:**
    *   Graceful handling of network errors, API failures, and invalid responses from external services.
    *   Retry mechanisms for Telegram login.
    *   Clear error messages sent back to the user.
*   **Configuration:**
    *   Uses a `.env` file for easy configuration of API keys, endpoints, and bot settings.
*   **Asynchronous:** Built with `asyncio` for efficient non-blocking operations.

## 🔧 Prerequisites

*   Python 3.8+
*   pip (Python package installer)
*   A Telegram API ID and Hash (get them from [my.telegram.org](https://my.telegram.org/apps))
*   Access to LLM endpoints (OpenAI, custom, and/or a local "uncensored" model)
*   (Optional) The `zindex5` executable/script accessible in your system's PATH or in the bot's directory for the `/z` command.
*   (Optional) Access to the `https://md-to-pdf.fly.dev` service for PDF conversion (or you can replace this with your own).

## ⚙️ Setup & Installation

1.  **Clone the repository (if applicable) or save the script:**
    ```bash
    # If you have it in a git repo:
    # git clone <your-repo-url>
    # cd <your-repo-directory>
    ```
    Save the provided Python script as `autobot.py` (or any name you prefer).

2.  **Create a virtual environment (recommended):**
    ```bash
    python -m venv venv
    source venv/bin/activate  # On Windows: venv\Scripts\activate
    ```

3.  **Install dependencies:**
    Create a `requirements.txt` file with the following content:
    ```txt
    python-dotenv
    requests
    openai
    telethon
    ```
    Then install them:
    ```bash
    pip install -r requirements.txt
    ```

4.  **Create a `.env` file:**
    Create a file named `.env` in the same directory as your script and populate it with your credentials and settings. **Do not commit this file to version control if it contains sensitive information.**

    ```env
    # Telegram API Credentials
    TG_API_ID="YOUR_TELEGRAM_API_ID"
    TG_API_HASH="YOUR_TELEGRAM_API_HASH"
    PHONE_NUMBER="YOUR_PHONE_NUMBER_WITH_COUNTRY_CODE" # e.g., +1234567890
    SESSION_NAME="my_autobot_session" # Optional: Name for the session file

    # Bot Configuration
    MY_USERNAME="YOUR_BOTS_TELEGRAM_USERNAME" # Your bot's (or your own if running as user) Telegram username (without @)

    # LLM & Service Endpoints
    LLM_CONTEXT_PATH="./llm_context.txt" # Path to a file containing context for the !cb command
    COMMAND_BASE_URL="YOUR_LLM_COMMAND_API_BASE_URL" # e.g., http://localhost:5000
    LLMM1="model-name-for-q-command" # Model name for the !q command
    OZZO_API_KEY="YOUR_OZZO_SERVICE_API_KEY" # API key for OZZO_GETCONT
    OZZO_GETCONT="YOUR_OZZO_CONTENT_FETCH_ENDPOINT" # Endpoint for !w command data source
    OPENAI_API_KEY="sk-YOUR_OPENAI_API_KEY"
    UNCENSORED_AI_URL="http://0.0.0.0:1234" # URL for your local/uncensored LLM API (e.g., Ollama, LM Studio)

    # Optional:
    # LOGS_DIR="./logs" # Default is ./logs
    # MAX_UNIQUE_LINES=500 # For /z command
    # MAX_LOGIN_ATTEMPTS=3
    # SAVE_INTERVAL_SECONDS=300 # 5 minutes
    ```

5.  **Configure Authorized Users:**
    In the Python script, modify the `AUTHORIZED_IDS` and `UNAUTHORIZED_IDS` lists:
    ```python
    AUTHORIZED_IDS = [
        "YOUR_TELEGRAM_USER_ID_1", # Replace with actual numeric Telegram User IDs
        "YOUR_TELEGRAM_USER_ID_2"
    ]
    UNAUTHORIZED_IDS = [ # For specific commands like /z
        "USER_ID_TO_BLOCK_FROM_Z"
    ]
    ```
    *You can find your User ID by talking to a bot like `@userinfobot` on Telegram.*

6.  **Prepare `LLM_CONTEXT_PATH` file (optional):**
    If you plan to use the `!cb` command, create the file specified in `LLM_CONTEXT_PATH` (e.g., `llm_context.txt`) and fill it with the desired context for your LLM.

7.  **Ensure `zindex5` is accessible (optional):**
    If you use the `/z` command, make sure the `zindex5` executable or script is in your system's PATH or in the same directory as the bot script (or adjust the path in `handle_z_command`).

## 🚀 Running the Bot

1.  **Activate your virtual environment (if you created one):**
    ```bash
    source venv/bin/activate  # On Windows: venv\Scripts\activate
    ```

2.  **Run the script:**
    ```bash
    python autobot.py
    ```

3.  **First-Time Login:**
    *   The bot will attempt to log in using your phone number.
    *   Telegram will send a login code to your Telegram account (not SMS, unless it's the very first login for that API ID/Hash on a new device/setup).
    *   Enter the code when prompted in the console.
    *   If you have 2-Factor Authentication (Cloud Password) enabled on Telegram, the script currently **does not handle this** and will print a message. You'll need to modify the script's login sequence to include `client.sign_in(password='YOUR_2FA_PASSWORD')` or temporarily disable 2FA for the initial login. The script mentions: `raise RuntimeError("2FA password needed. Update code to handle this.")`.
    *   Once successfully logged in, a session file (e.g., `my_autobot_session.session`) will be created, so you won't need to enter the code every time.

4.  The bot will print "Login successful!" and "Bot is running. Press Ctrl+C to stop."

## 📋 Commands

Bot commands are triggered by specific prefixes. Only users in `AUTHORIZED_IDS` can use these commands.

| Command         | Prefix   | Description                                                                                             | Usage Example                                       |
| :-------------- | :------- | :------------------------------------------------------------------------------------------------------ | :-------------------------------------------------- |
| **Query**       | `!q`     | Sends a prompt to a configured LLM endpoint (model `LLMM1`).                                            | `!q What is the capital of France?`                 |
| **Context Query** | `!cb`    | Sends a prompt to an LLM, including context from `LLM_CONTEXT_PATH`.                                    | `!cb Based on our discussion, what's the next step?`  |
| **Web Content** | `!w`     | Fetches content from a URL using `OZZO_GETCONT`. <br> Flags: `+pdf` (get PDF), `+inline` (show in chat). | `!w https://example.com` <br> `!w +pdf https://example.com` <br> `!w +pdf +inline https://example.com` |
| **ARF!**        | `!arf`   | The bot replies with "ARF!" three times.                                                                | `!arf`                                              |
| **Summarize**   | `!cai`   | Summarizes recent chat messages from the current chat using the uncensored/local LLM. <br> Flag: `-d N` (summarize N days ago). | `!cai` <br> `!cai -d 1` (yesterday's messages)         |
| **Search Logs** | `!search`| Searches logged messages in the current chat for a keyword (case-insensitive).                          | `!search project update`                            |
| **Chat Stats**  | `!stats` | Displays statistics for the current day's messages in the chat.                                         | `!stats`                                            |
| **Fuzzy Search**| `!fsearch`| Performs a fuzzy search in logged messages for a query.                                                 | `!fsearch prjoect updte`                            |
| **Z Index**     | `/z`     | Searches an external index using the `./zindex5` command-line tool.                                     | `/z important keyword`                              |

**Reply-Chaining:**
If you reply directly to a message sent by the bot that was a response to a `!q` or `!cb` command, your reply text will be used as new input for that original command. For example, if the bot replied `!q Paris` to your query, and you reply to *that message* with `What about its population?`, the bot will effectively run `!q What about its population?`.

## 📁 File Structure

```
your-bot-directory/
├── autobot.py            # The main bot script
├── .env                  # Environment variables (GITIGNORED)
├── requirements.txt      # Python dependencies
├── llm_context.txt       # (Optional) Context file for !cb command
├── zindex5               # (Optional) External executable for /z command
├── logs/                 # Directory for message logs
│   └── chat_name-YYYY-MM-DD.json # Example log file
└── my_autobot_session.session # Telegram session file (created after login)
```

## 🛡️ Error Handling

The bot incorporates several error handling mechanisms:
*   **Network Issues:** Catches `requests.exceptions.RequestException` for LLM calls and `md-to-pdf` service, providing user-friendly messages.
*   **HTTP Errors:** Handles `requests.exceptions.HTTPError` from external services.
*   **API Errors:** Manages `openai.error.OpenAIError` and checks for empty or invalid JSON responses from LLMs.
*   **Login Failures:** Implements a retry mechanism for Telegram login, with specific handling for `PHONE_CODE_INVALID` and `FLOOD_WAIT` errors.
*   **File Operations:** Includes `try-except` blocks for reading and writing log files.
*   **Missing Configuration:** Checks for required environment variables at startup.
*   **Command Failures:** Most command handlers will reply with an error message if something goes wrong (e.g., no URL provided, no data found).

## 💡 Important Notes

*   **2FA/Cloud Password:** The current script requires manual intervention or modification if your Telegram account has 2FA enabled. See the "First-Time Login" section.
*   **External Dependencies:** The `!w +pdf` functionality relies on `https://md-to-pdf.fly.dev`. The `/z` command relies on an external `zindex5` tool. Ensure these are available and configured if you intend to use these features.
*   **API Keys & Costs:** Be mindful of API rate limits and potential costs associated with using OpenAI or other paid LLM services.
*   **`UNCENSORED_AI_URL`:** This typically points to a locally hosted LLM service like Ollama (e.g., `http://localhost:11434`) or LM Studio. Ensure the specified model (`solar-10.7b-instruct-v1.0-uncensored` in the script) is available on that service.
*   **Log File Size:** Log files can grow large over time. Consider implementing log rotation or archiving if necessary for long-term use.

## 🤝 Contributing

Feel free to fork the project, make improvements, and submit pull requests!

---

This README should provide a good overview and guide for setting up and using your Telegram Autobot. Remember to replace placeholders with your actual information.
```
