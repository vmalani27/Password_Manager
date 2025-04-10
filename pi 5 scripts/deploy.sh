#!/bin/bash

# Configuration
REMOTE_USER="pi"
REMOTE_HOST="192.168.1.100"
REMOTE_DIR="/home/pi/password_manager"
LOCAL_DIR="."

# Create remote directory if it doesn't exist
ssh $REMOTE_USER@$REMOTE_HOST "mkdir -p $REMOTE_DIR"

# Transfer files
echo "Transferring files to remote server..."
scp $LOCAL_DIR/app.py $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/
scp $LOCAL_DIR/password_analyzer.py $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/
scp $LOCAL_DIR/requirements.txt $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/
scp $LOCAL_DIR/CHANGELOG.md $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/
scp -r $LOCAL_DIR/templates $REMOTE_USER@$REMOTE_HOST:$REMOTE_DIR/

# Create and activate virtual environment
echo "Setting up Python environment..."
ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && \
    python3 -m venv venv && \
    source venv/bin/activate && \
    pip install -r requirements.txt"

# Backup existing database if it exists
echo "Backing up existing database..."
ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && \
    if [ -f passwords.db ]; then \
        cp passwords.db passwords.db.backup; \
        echo 'Database backed up as passwords.db.backup'; \
    fi"

# Initialize database
echo "Initializing database..."
ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && \
    source venv/bin/activate && \
    python3 -c 'from app import init_db; init_db()'"

# Start the application in the background
echo "Starting the application..."
ssh $REMOTE_USER@$REMOTE_HOST "cd $REMOTE_DIR && \
    source venv/bin/activate && \
    nohup python3 app.py > app.log 2>&1 &"

echo "Deployment completed successfully!"
echo "Application is running at http://$REMOTE_HOST:5000"
echo "Logs are available at $REMOTE_DIR/app.log" 