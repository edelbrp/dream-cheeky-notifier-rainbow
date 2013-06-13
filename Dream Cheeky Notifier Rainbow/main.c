//
//	File:		main.c
//
//	Contains:	Dream Cheeky Notifier Rainbow generator based on Apple's HID LED test tool.  When run, the first Dream
//				Cheeky webmail notifier device found on the USB bus will be run in a infinite loop of color changes.
//
//	Copyright:	Copyright (c) 2007 Apple Inc., and (c) 2013 Philip Edelbrock <phil@philedelbrock.com>, All Rights Reserved
//
//  Usage:		Permission is given by Apple and Phil Edelbrock to reuse/repurpose this code without restriction.  Please
//				refer to Apple's original HID LED test tool source for further usage/disclaimer information.
//
#pragma mark -
#pragma mark * complation directives *
// ----------------------------------------------------

#ifndef FALSE
#define FALSE 0
#define TRUE !FALSE
#endif

// ****************************************************
#pragma mark -
#pragma mark * includes & imports *
// ----------------------------------------------------

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>

// ****************************************************
#pragma mark -
#pragma mark * typedef's, struct's, enums, defines, etc. *
// ----------------------------------------------------
// function to create a matching dictionary for usage page & usage
static CFMutableDictionaryRef hu_CreateMatchingDictionaryUsagePageUsage( Boolean isDeviceNotElement,
																		UInt32 inUsagePage,
																		UInt32 inUsage )
{
	// create a dictionary to add usage page / usages to
	CFMutableDictionaryRef result = CFDictionaryCreateMutable( kCFAllocatorDefault,
															  0,
															  &kCFTypeDictionaryKeyCallBacks,
															  &kCFTypeDictionaryValueCallBacks );
	
	if ( result ) {
		if ( inUsagePage ) {
			// Add key for device type to refine the matching dictionary.
			CFNumberRef pageCFNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &inUsagePage );
			
			if ( pageCFNumberRef ) {
				if ( isDeviceNotElement ) {
					CFDictionarySetValue( result, CFSTR( kIOHIDDeviceUsagePageKey ), pageCFNumberRef );
				} else {
					CFDictionarySetValue( result, CFSTR( kIOHIDElementUsagePageKey ), pageCFNumberRef );
				}
				CFRelease( pageCFNumberRef );
				
				// note: the usage is only valid if the usage page is also defined
				if ( inUsage ) {
					CFNumberRef usageCFNumberRef = CFNumberCreate( kCFAllocatorDefault, kCFNumberIntType, &inUsage );
					
					if ( usageCFNumberRef ) {
						if ( isDeviceNotElement ) {
							CFDictionarySetValue( result, CFSTR( kIOHIDDeviceUsageKey ), usageCFNumberRef );
						} else {
							CFDictionarySetValue( result, CFSTR( kIOHIDElementUsageKey ), usageCFNumberRef );
						}
						CFRelease( usageCFNumberRef );
					} else {
						fprintf( stderr, "%s: CFNumberCreate( usage ) failed.", __PRETTY_FUNCTION__ );
					}
				}
			} else {
				fprintf( stderr, "%s: CFNumberCreate( usage page ) failed.", __PRETTY_FUNCTION__ );
			}
		}
	} else {
		fprintf( stderr, "%s: CFDictionaryCreateMutable failed.", __PRETTY_FUNCTION__ );
	}
	return result;
}	// hu_CreateMatchingDictionaryUsagePageUsage

// function to get a long device property
// returns FALSE if the property isn't found or can't be converted to a long
static Boolean IOHIDDevice_GetLongProperty(
										   IOHIDDeviceRef inDeviceRef,     // the HID device reference
										   CFStringRef inKey,              // the kIOHIDDevice key (as a CFString)
										   long * outValue)                // address where to return the output value
{
    Boolean result = FALSE;
	
    CFTypeRef tCFTypeRef = IOHIDDeviceGetProperty(inDeviceRef, inKey);
    if (tCFTypeRef) {
        // if this is a number
        if (CFNumberGetTypeID() == CFGetTypeID(tCFTypeRef)) {
            // get its value
            result = CFNumberGetValue((CFNumberRef) tCFTypeRef, kCFNumberSInt32Type, outValue);
        }
    }
    return result;
}   // IOHIDDevice_GetLongProperty

#define round(x) ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))

//
//	Sets the color of the Dream Cheeky webmail notifier device pointed to by the_device.
//	Note: This function already assumes that the device has it's output LEDs enabled (see below in main for how that was done).
//
void static SetColor(IOHIDDeviceRef the_device, float r, float g, float b) {
	// I found these values for RGB to be close to white: 27 31 15.  Change all values to 31 if you want full brightness.
	unsigned char report[8];
	if (TRUE) { // Normalize whitepoint, set to false if you want brighter but not normalized intensities
		report[0] = round(r * 27); // set brightness on LEDs to r, g, & b
		report[1] = round(g * 31);
		report[2] = round(b * 15);
	} else {
		report[0] = round(r * 31);
		report[1] = round(g * 31);
		report[2] = round(b * 31);
	}
	report[3] = 0x00;
	report[4] = 0x00;
	report[5] = 0x00;
	report[6] = 0x1A;
	report[7] = 0x05;
	
	IOHIDDeviceSetReport(
                         the_device,          // IOHIDDeviceRef for the HID device
                         kIOHIDReportTypeInput,   // IOHIDReportType for the report (input, output, feature)
                         0,           // CFIndex for the report ID
                         report,             // address of report buffer
                         8);      // length of the report
}

int main( int argc, const char * argv[] )
{
	
	// create a IO HID Manager reference
	IOHIDManagerRef tIOHIDManagerRef = IOHIDManagerCreate( kCFAllocatorDefault, kIOHIDOptionsTypeNone );
	// Create a device matching dictionary  Note: I'm not sure we used matchingCFDictRef for anything practical below?
	CFDictionaryRef matchingCFDictRef = hu_CreateMatchingDictionaryUsagePageUsage( TRUE,
																				  kHIDPage_GenericDesktop,
																				  0x10 );
	// set the HID device matching dictionary
	IOHIDManagerSetDeviceMatching( tIOHIDManagerRef, matchingCFDictRef );
	if ( matchingCFDictRef ) {
		CFRelease( matchingCFDictRef );
	}
	
	// Now open the IO HID Manager reference
	IOReturn tIOReturn = IOHIDManagerOpen( tIOHIDManagerRef, kIOHIDOptionsTypeNone );
    // Note: We're not doing anything with tIOReturn here, so there might be a compiler warning.
    
	// and copy out its devices
	CFSetRef deviceCFSetRef = IOHIDManagerCopyDevices( tIOHIDManagerRef );
	
	// how many devices in the set?
	CFIndex deviceIndex, deviceCount = CFSetGetCount( deviceCFSetRef );
	
	// allocate a block of memory to extact the device ref's from the set into
	IOHIDDeviceRef * tIOHIDDeviceRefs = malloc( sizeof( IOHIDDeviceRef ) * deviceCount );
	
	// now extract the device ref's from the set
	CFSetGetValues( deviceCFSetRef, (const void **) tIOHIDDeviceRefs );
	
	// before we get into the device loop we'll setup our element matching dictionary (Note: we don't do element matching anymore)
	matchingCFDictRef = hu_CreateMatchingDictionaryUsagePageUsage( FALSE, 0, 0 );
	
	for ( deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++ ) {
		
		// Make sure this is the Dream Cheeky Webemail notifier device that has right vendor and product ids
		long vendor_id = 0;
		IOHIDDevice_GetLongProperty(tIOHIDDeviceRefs[deviceIndex], CFSTR(kIOHIDVendorIDKey), &vendor_id);
		long product_id = 0;
		IOHIDDevice_GetLongProperty(tIOHIDDeviceRefs[deviceIndex], CFSTR(kIOHIDProductIDKey), &product_id);
		if ((vendor_id != 0x1D34 ) || (product_id != 0x0004 )) {
			printf("	skipping device %p.\n", tIOHIDDeviceRefs[deviceIndex] );
			continue;	// ...skip it
		}
		
		printf( "	 device = %p.\n", tIOHIDDeviceRefs[deviceIndex] );
		
		unsigned char report[8];
		report[0] = 0x1F; // turn on LEDs
		report[1] = 0x02;
		report[2] = 0x00;
		report[3] = 0x5F;
		report[4] = 0x00;
		report[5] = 0x00;
		report[6] = 0x1A;
		report[7] = 0x03;
		
		// Give the device the command to enable LEDs to light up.
        // Note: We're not doing anything with tIOReturn here, so there might be a compiler warning.
		IOReturn  tIOReturn = IOHIDDeviceSetReport(
												   tIOHIDDeviceRefs[deviceIndex],          // IOHIDDeviceRef for the HID device
												   kIOHIDReportTypeInput,   // IOHIDReportType for the report (input, output, feature)
												   0,           // CFIndex for the report ID
												   report,             // address of report buffer
												   8);      // length of the report
		
		float r=1.0; // we start with yellow
		float g=1.0;
		float b=0.0;
		int leg = 0;
		float interval = (1.0 / (31.0 * 6.0)); // smallest interval
		
		while (TRUE) { // Yes, sorry, we don't actually gracefully release things down below, and we're latching onto the first device and ignoring any others.
			if (leg == 0) { // yellow to red and travel the edges of the color square corners
				g -= interval;
				if (g <= 0) {
					g = 0;
					leg = 1;
				}
			}
			if (leg == 1) {
				b += interval;
				if (b >= 1) {
					b = 1;
					leg = 2;
				}
			}
			if (leg == 2) {
				r -= interval;
				if (r <= 0) {
					r = 0;
					leg = 3;
				}
			}
			if (leg == 3) {
				g += interval;
				if (g >= 1) {
					g = 1;
					leg = 4;
				}
			}
			if (leg == 4) {
				b -= interval;
				if (b <= 0) {
					b = 0;
					leg = 5;
				}
			}
			if (leg == 5) {
				r += interval;
				if (r >= 1) {
					r = 1;
					leg = 6;
				}
			}
			if (leg > 5) {
				leg = 0;
			}
			SetColor(tIOHIDDeviceRefs[deviceIndex], r, g, b);
			usleep( 2000 ); // The device stutters, it honestly doesn't matter how fast you ping it, just don't load the host
            //			SetColor(tIOHIDDeviceRefs[deviceIndex], 0, 0, 0);
            //			usleep( 2000 );
		}
		
	next_device: ;
		continue;
	}
	
	
	if ( tIOHIDManagerRef ) {
		CFRelease( tIOHIDManagerRef );
	}
	
	if ( matchingCFDictRef ) {
		CFRelease( matchingCFDictRef );
	}
} /* main */
