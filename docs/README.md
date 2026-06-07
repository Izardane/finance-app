# Finance Dashboard — Mobile PWA

A mobile-friendly Progressive Web App that syncs with your C++ desktop Finance Dashboard via Google Drive.

## How It Works

- The PWA stores portfolio data in `portfolio.txt` on your Google Drive.
- Your desktop C++ app should also read/write `portfolio.txt` from your Google Drive folder (see "Desktop Sync" below).
- The PWA downloads the file from Drive on open and uploads on every save.
- Both devices share the same file, giving you **bidirectional sync**.

## Setup Guide (One-Time, ~10 minutes)

### Step 1: Enable Google Drive API

1. Go to https://console.cloud.google.com
2. Make sure you are in the **Finance App** project you created (check the project dropdown at the top).
3. In the left sidebar, click **"APIs & Services"** → **"Library"**.
4. In the search bar, type **"Google Drive API"**.
5. Click on **"Google Drive API"** in the results.
6. Click the **"Enable"** button.
7. Wait a few seconds — you should see "API enabled" with a green checkmark.

### Step 2: Create OAuth 2.0 Credentials

1. Still in the Google Cloud Console, click **"APIs & Services"** → **"Credentials"** in the left sidebar.
2. Click the **"+ CREATE CREDENTIALS"** button at the top.
3. Select **"OAuth client ID"**.
4. If asked to configure the consent screen first:
   - Click **"CONFIGURE CONSENT SCREEN"**.
   - Choose **"External"** user type and click **"CREATE"**.
   - Fill in **App name**: `Finance Dashboard`
   - Fill in **User support email**: (your email)
   - Fill in **Developer contact information**: (your email)
   - Click **"SAVE AND CONTINUE"** (skip scopes, skip test users).
   - Click **"BACK TO DASHBOARD"**.
   - Now go back to **"Credentials"** → **"+ CREATE CREDENTIALS"** → **"OAuth client ID"**.
5. For **Application type**, choose **"Web application"**.
6. **Name**: `Finance Dashboard Web`
7. Under **"Authorized JavaScript origins"**, click **"+ ADD URI"** and add:
   - `http://localhost`
   - `https://YOUR_USERNAME.github.io` (replace with your GitHub username — you'll get this URL later)
8. Click **"CREATE"**.
9. A popup will show your **Client ID** and **Client Secret**. Copy the **Client ID** string (looks like `123456789-xxxxx.apps.googleusercontent.com`).

### Step 3: Configure the PWA

1. Open `app.js` in a text editor.
2. Find this line near the top:
   ```js
   client_id: 'YOUR_CLIENT_ID',
   ```
3. Replace `YOUR_CLIENT_ID` with the Client ID you copied:
   ```js
   client_id: '123456789-xxxxx.apps.googleusercontent.com',
   ```
4. Save the file.

### Step 4: Push to GitHub and Enable Pages

1. Go to https://github.com/new and create a **new repository** (name it whatever you like, e.g. `finance-app`). Do NOT check "Add a README" or ".gitignore" — keep it empty.
2. Come back to your terminal. I'll help you push the code up.

### Step 5: Update OAuth Redirects

1. Go back to https://console.cloud.google.com → **APIs & Services** → **Credentials**.
2. Click on your OAuth 2.0 Client ID.
3. Under **"Authorized JavaScript origins"**, add:
   - `https://YOUR_USERNAME.github.io`
4. Click **"Save"**.

## How to Install on Your Phone

### Android (Chrome)

1. Open Chrome and visit your deployed PWA URL.
2. Tap the menu (three dots) → **"Add to Home screen"**.
3. Tap **"Install"**.
4. The app will now appear on your home screen like a native app.

### iPhone (Safari)

1. Open Safari and visit your deployed PWA URL.
2. Tap the **Share button** (square with arrow).
3. Scroll down and tap **"Add to Home Screen"**.
4. Tap **"Add"** in the top right.
5. The app will now appear on your home screen.

## Desktop Sync Setup

To make your desktop C++ app sync with Google Drive:

### Option A: Google Drive for Desktop (Recommended)

1. Download and install **Google Drive for Desktop** from https://drive.google.com/drive/download
2. Sign in with the same Google account.
3. Choose to sync a folder to your computer (default is `~/Google Drive/`).
4. Place your `portfolio.txt` data file in this folder.
5. When running the C++ desktop app, save the portfolio to this path, for example:
   - On Linux/macOS: `/home/yourname/Google Drive/portfolio.txt`
   - On Windows: `C:\Users\yourname\Google Drive\portfolio.txt`
6. The PWA will read/write the same file on Google Drive.
7. **Sync flow**: Edit on desktop → Google Drive syncs to cloud → Open PWA → PWA downloads latest → Edit on PWA → PWA uploads → Google Drive syncs to desktop.

### Option B: Manual

Periodically export from the C++ app (Save & Generate) and upload the resulting `portfolio.txt` to Google Drive. On the PWA, use Settings → Import to load it.

## Usage

| Feature | How |
|---------|-----|
| **Add a category** | Categories tab → fill name + amount → choose mode → Save |
| **Edit a category** | Tap the pencil icon next to a category |
| **Add to existing** | Use "Add to existing" radio mode |
| **Delete a category** | Tap the X icon |
| **Add earmark** | Earmarks tab → select a category → fill purpose + amount → Save |
| **Edit earmark** | Tap the pencil icon next to an earmark |
| **Delete earmark** | Tap the X icon |
| **Sync** | Automatic on every save |

## File Format

The app uses the same pipe-delimited format as the C++ desktop app:

```
ASSET|Bank FD|500000
EARMARK|Bank FD|Vacation|100000
```

Amounts are in paise (cents). 500000 = ₹5,000.00.

## Offline Mode

If you tap "Continue without syncing", the app works fully offline using your browser's local storage. You can connect Google Drive later from the Settings tab.
