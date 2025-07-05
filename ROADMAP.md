# gui/main_window.py - Add backend integration (append to existing file)

# Add these imports at the top
import sys
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from signal_exporter.config import Config, ExportConfig
from signal_exporter.exporter import SignalExporter
from signal_exporter.formatters import get_formatter
from signal_exporter.cloud import CloudBackup

class BackendIntegration:
    """Bridge between GUI and backend"""
    
    def __init__(self):
        self.config = Config()
        
    def test_connection(self) -> tuple[bool, str]:
        """Test Signal database connection"""
        try:
            config = self.config._config
            exporter = SignalExporter(config)
            with exporter.db:
                convs = exporter.db.get_conversations()
            return True, f"Connected! Found {len(convs)} conversations"
        except Exception as e:
            return False, str(e)
    
    def export_conversations(self, format_type: str = 'json', 
                           selected_convs: list = None) -> tuple[bool, str]:
        """Export conversations"""
        try:
            config = self.config._config
            config.export_format = format_type
            
            exporter = SignalExporter(config)
            data = exporter.export_all_conversations()
            
            # Format data
            formatter = get_formatter(format_type)
            formatted = formatter.format(data)
            
            # Save to file
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"signal_export_{timestamp}.{format_type}"
            output_path = config.output_dir / filename
            
            config.output_dir.mkdir(parents=True, exist_ok=True)
            with open(output_path, 'w') as f:
                f.write(formatted)
                
            return True, str(output_path)
            
        except Exception as e:
            return False, str(e)
    
    def upload_to_cloud(self, file_path: str, provider: str = 'sftp') -> tuple[bool, str]:
        """Upload file to cloud"""
        try:
            backup = CloudBackup(provider)
            success = backup.upload(Path(file_path))
            
            if success:
                return True, "Upload successful"
            else:
                return False, "Upload failed"
                
        except Exception as e:
            return False, str(e)

# Modify MainWindow.__init__ to add backend
def enhanced_init(self):
    # ... existing init code ...
    
    # Add backend integration
    self.backend = BackendIntegration()
    
    # Connect test button
    if hasattr(self, 'test_button'):
        self.test_button.clicked.connect(self.test_backend_connection)
        
def test_backend_connection(self):
    """Test backend connection"""
    success, message = self.backend.test_connection()
    
    if success:
        QMessageBox.information(self, "Success", message)
    else:
        QMessageBox.warning(self, "Connection Failed", message)

# Add methods to MainWindow class
MainWindow.test_backend_connection = test_backend_connection