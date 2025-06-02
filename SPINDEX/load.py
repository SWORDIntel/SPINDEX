import sqlite3
import json
import pandas as pd

# Load JSON data
with open("examplecb.json", "r") as file:
    data = json.load(file)

# Connect to SQLite
conn = sqlite3.connect(":memory:")
cursor = conn.cursor()

# Create table
cursor.execute("""
CREATE TABLE user_data (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    document_id TEXT,
    full_name TEXT,
    email TEXT,
    country TEXT,
    account_signup_time DATETIME,
    account_modification_time DATETIME,
    social_linkedin TEXT,
    email_domain TEXT,
    email_username TEXT,
    severity INTEGER
);
""")
conn.commit()

# Insert data
for record in data:
    cursor.execute("""
    INSERT INTO user_data (
        document_id, full_name, email, country, account_signup_time, 
        account_modification_time, social_linkedin, email_domain, email_username, severity
    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    """, (
        record.get("document_id"),
        record.get("full_name"),
        record.get("email"),
        record.get("country"),
        record.get("account_signup_time"),
        record.get("account_modification_time"),
        ", ".join(record.get("social_linkedin", [])),
        record.get("email_domain"),
        record.get("email_username"),
        record.get("severity")
    ))

conn.commit()

# Query data
df = pd.read_sql_query("SELECT * FROM user_data;", conn)
print(df)
