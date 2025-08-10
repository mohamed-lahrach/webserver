// JavaScript Test File
console.log("JavaScript file loaded successfully!");

document.addEventListener('DOMContentLoaded', function() {
    console.log("DOM fully loaded and parsed");
    
    // Test MIME type detection
    var testElement = document.getElementById('mime-test');
    if (testElement) {
        testElement.innerHTML = '✅ JavaScript MIME type working correctly!';
        testElement.style.color = '#28a745';
        testElement.style.fontWeight = 'bold';
    }
    
    // Add click handler for testing
    var buttons = document.querySelectorAll('.test-button');
    buttons.forEach(function(button) {
        button.addEventListener('click', function() {
            alert('JavaScript is working! MIME type: application/javascript');
        });
    });
});
