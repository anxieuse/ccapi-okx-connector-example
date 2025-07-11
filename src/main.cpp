#include "ccapi_cpp/ccapi_session.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <numeric>
#include <iomanip>

namespace ccapi {

Logger* Logger::logger = nullptr;  // This line is needed.

class MyEventHandler : public EventHandler {
 public:
  MyEventHandler(bool marketDataOnly = false) : orderCount(0), lastOrderTime(std::chrono::high_resolution_clock::now()), marketDataOnly(marketDataOnly), messageCount(0) {}

  void processEvent(const Event& event, Session* sessionPtr) override {
    auto now = std::chrono::high_resolution_clock::now();
    
    std::cout << "\n🔔 EVENT RECEIVED: " << (event.getType() == Event::Type::SUBSCRIPTION_STATUS ? "SUBSCRIPTION_STATUS" : 
                                            event.getType() == Event::Type::SUBSCRIPTION_DATA ? "SUBSCRIPTION_DATA" :
                                            event.getType() == Event::Type::RESPONSE ? "RESPONSE" : "OTHER") << std::endl;
    
    if (event.getType() == Event::Type::SUBSCRIPTION_STATUS) {
      std::cout << "📋 SUBSCRIPTION_STATUS Details:\n" << event.toStringPretty(2, 2) << std::endl;
      
              // Check if subscription failed
        for (const auto& message : event.getMessageList()) {
          if (message.getType() == Message::Type::SUBSCRIPTION_FAILURE_DUE_TO_CONNECTION_FAILURE) {
            std::cerr << "\n⚠️  PRIVATE WEBSOCKET SUBSCRIPTION FAILED!" << std::endl;
            std::cerr << "Possible causes:" << std::endl;
            std::cerr << "  1. Invalid API credentials" << std::endl;
            std::cerr << "  2. API key lacks trading permissions" << std::endl;
            std::cerr << "  3. Demo/sandbox credentials used with production endpoint" << std::endl;
            std::cerr << "  4. IP whitelist restrictions" << std::endl;
            std::cerr << "\nSwitching to MARKET DATA ONLY mode..." << std::endl;
            marketDataOnly = true;
            return;
          }
        }
    } 
    else if (event.getType() == Event::Type::SUBSCRIPTION_DATA) {
      // Handle both market data updates and order updates
      for (const auto& message : event.getMessageList()) {
        
        // Log all message types and details for debugging
        std::cout << "📬 Message Type: " << (int)message.getType() << " | " 
                 << (message.getType() == Message::Type::MARKET_DATA_EVENTS_MARKET_DEPTH ? "MARKET_DATA_EVENTS_MARKET_DEPTH" :
                     message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE ? "EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE" :
                     "OTHER_MESSAGE_TYPE") << std::endl;
        
        // Check correlation IDs to see if this is order-related
        const auto& correlationIds = message.getCorrelationIdList();
        for (const auto& corrId : correlationIds) {
          std::cout << "🔗 Correlation ID: " << corrId << std::endl;
          if (corrId.find("test_order_") != std::string::npos || corrId.find("gZvs2qhN") != std::string::npos) {
            std::cout << "🎯 FOUND ORDER-RELATED CORRELATION ID: " << corrId << std::endl;
          }
        }
        
        if (message.getType() == Message::Type::EXECUTION_MANAGEMENT_EVENTS_ORDER_UPDATE) {
          // Handle order updates for latency measurement
          auto localReceiveTime = std::chrono::high_resolution_clock::now();
          auto serverProcessTime = message.getTime(); // Server timestamp when OKX processed the order
          // Print receive timestamp in ISO format
          auto receiveTimeT = std::chrono::system_clock::now();
          std::time_t receiveTimeC = std::chrono::system_clock::to_time_t(receiveTimeT);
          std::cout << "⏰ Order update received timestamp (local, ISO): " << std::put_time(std::gmtime(&receiveTimeC), "%Y-%m-%dT%H:%M:%SZ") << std::endl;
          // Print message.getTime() and message.getTimeReceived() in ISO format
          std::cout << "⏰ Order update message.getTime() (exchange/server, ISO): " << message.getTimeISO() << std::endl;
          std::cout << "⏰ Order update message.getTimeReceived() (CCAPI/local, ISO): " << message.getTimeReceivedISO() << std::endl;
          
          std::cout << "💰 ORDER UPDATE RECEIVED:\n" << message.toStringPretty(2, 2) << std::endl;
          
          for (const auto& element : message.getElementList()) {
            const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
            
            std::cout << "📊 Order Update Fields:" << std::endl;
            for (const auto& pair : elementNameValueMap) {
              std::cout << "  " << pair.first << " = " << pair.second << std::endl;
            }
            
            std::string clientOrderId = element.getValue("CLIENT_ORDER_ID");
            std::string status = element.getValue("STATUS");
            
            auto it = orderTimestamps.find(clientOrderId);
            if (it != orderTimestamps.end()) {
              // **CALCULATE BOTH LATENCIES**
              
              // 1. Total Round-Trip Latency (current approach)
              auto totalLatency = std::chrono::duration_cast<std::chrono::microseconds>(localReceiveTime - it->second);
              
              // 2. Server Processing Latency (your improved approach)
              auto serverLatency = std::chrono::duration_cast<std::chrono::microseconds>(serverProcessTime - it->second);
              
              // 3. Network Delay (difference between the two)
              auto networkDelay = std::chrono::duration_cast<std::chrono::microseconds>(localReceiveTime - serverProcessTime);
              
              // Store the server latency (more accurate) for statistics
              latencies.push_back(serverLatency.count());
              
              std::cout << "⚡ DUAL LATENCY ANALYSIS - Client ID: " << clientOrderId 
                       << ", Status: " << status << std::endl;
              std::cout << "  📊 Server Processing Latency: " << serverLatency.count() 
                       << " μs (" << (serverLatency.count() / 1000.0) << " ms)" << std::endl;
              std::cout << "  🌐 Total Round-Trip Latency: " << totalLatency.count() 
                       << " μs (" << (totalLatency.count() / 1000.0) << " ms)" << std::endl;
              std::cout << "  📡 Network Delay: " << networkDelay.count() 
                       << " μs (" << (networkDelay.count() / 1000.0) << " ms)" << std::endl;
              std::cout << "  🎯 Improvement: " << (totalLatency.count() - serverLatency.count()) 
                       << " μs (" << ((totalLatency.count() - serverLatency.count()) / 1000.0) << " ms) faster measurement" << std::endl;
              
              orderTimestamps.erase(it);
            } else {
              std::cout << "❓ CLIENT_ORDER_ID not found in pending orders: " << clientOrderId << std::endl;
            }
          }
        } 
        
        // Check ANY subscription data that might be order-related
        else if (correlationIds.size() > 0 && (correlationIds[0].find("gZvs2qhN") != std::string::npos)) {
          // This is from our order subscription channel
          std::cout << "🎯 ORDER CHANNEL SUBSCRIPTION DATA:\n" << message.toStringPretty(2, 2) << std::endl;
          
          auto responseTime = std::chrono::high_resolution_clock::now();
          
          // Look through all elements for potential order data
          for (const auto& element : message.getElementList()) {
            const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
            
            std::cout << "📊 Order Channel Fields:" << std::endl;
            for (const auto& pair : elementNameValueMap) {
              std::cout << "  " << pair.first << " = " << pair.second << std::endl;
            }
            
            // Try different field names for client order ID
            std::string clientOrderId;
            auto clientOrderIdIt = elementNameValueMap.find("CLIENT_ORDER_ID");
            if (clientOrderIdIt == elementNameValueMap.end()) {
              clientOrderIdIt = elementNameValueMap.find("clOrdId");
            }
            if (clientOrderIdIt == elementNameValueMap.end()) {
              clientOrderIdIt = elementNameValueMap.find("clientOrderId");
            }
            
            if (clientOrderIdIt != elementNameValueMap.end()) {
              clientOrderId = clientOrderIdIt->second;
              std::cout << "🔍 Found Client Order ID: " << clientOrderId << std::endl;
              
              auto it = orderTimestamps.find(clientOrderId);
              if (it != orderTimestamps.end()) {
                auto latency = std::chrono::duration_cast<std::chrono::microseconds>(responseTime - it->second);
                latencies.push_back(latency.count());
                
                std::cout << "⚡ LATENCY CALCULATED - Client ID: " << clientOrderId 
                         << ", Latency: " << latency.count() 
                         << " microseconds (" << (latency.count() / 1000.0) << " ms)" << std::endl;
                
                orderTimestamps.erase(it);
              } else {
                std::cout << "❓ CLIENT_ORDER_ID not found in pending orders: " << clientOrderId << std::endl;
              }
            } else {
              std::cout << "❓ No CLIENT_ORDER_ID found in order channel data" << std::endl;
            }
          }
        }
        
        else {
          // Handle market data updates
          messageCount++;
          
          std::cout << "MARKET DATA RECEIVED #" << messageCount << ":\n" << message.toStringPretty(2, 2) << std::endl;
          
          // Extract bid and ask prices from separate elements
          double bestBid = 0.0, bestAsk = 0.0;
          bool hasBid = false, hasAsk = false;
          
          for (const auto& element : message.getElementList()) {
            const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
            
            auto bidPriceIt = elementNameValueMap.find("BID_PRICE");
            auto askPriceIt = elementNameValueMap.find("ASK_PRICE");
            
            if (bidPriceIt != elementNameValueMap.end()) {
              try {
                bestBid = std::stod(bidPriceIt->second);
                hasBid = true;
              } catch (const std::exception& e) {
                std::cerr << "Error parsing bid price: " << e.what() << std::endl;
              }
            }
            
            if (askPriceIt != elementNameValueMap.end()) {
              try {
                bestAsk = std::stod(askPriceIt->second);
                hasAsk = true;
              } catch (const std::exception& e) {
                std::cerr << "Error parsing ask price: " << e.what() << std::endl;
              }
            }
          }
          
          // If we have both bid and ask, process the market data
          if (hasBid && hasAsk) {
            std::cout << "Market Data #" << messageCount << " - Bid: " << std::fixed << std::setprecision(2) 
                     << bestBid << ", Ask: " << bestAsk << ", Spread: " << (bestAsk - bestBid) << std::endl;
            
            // Only place orders if not in market data only mode
            if (!marketDataOnly) {
              // Place order every 10 seconds (like the original Python code), but place first order immediately
              auto timeSinceLastOrder = std::chrono::duration_cast<std::chrono::seconds>(now - lastOrderTime).count();
              if ((orderCount == 0 || timeSinceLastOrder >= 10) && orderCount < 3) { // Limit to 3 orders for testing
                placeTestOrder(sessionPtr, bestBid);
                lastOrderTime = now;
              }
            }
          }
        }
      }
    }
    else if (event.getType() == Event::Type::RESPONSE) {
      // Handle order responses (immediate REST API responses)
      std::cout << "💰 ORDER RESPONSE RECEIVED:\n" << event.toStringPretty(2, 2) << std::endl;
      
      // Process response for latency calculation
      for (const auto& message : event.getMessageList()) {
        std::cout << "📬 Response Message Type: " << (message.getType() == Message::Type::CREATE_ORDER ? "CREATE_ORDER" :
                                                      message.getType() == Message::Type::RESPONSE_ERROR ? "RESPONSE_ERROR" : "OTHER") << std::endl;
        
        for (const auto& element : message.getElementList()) {
          const std::map<std::string_view, std::string>& elementNameValueMap = element.getNameValueMap();
          
          std::cout << "📊 Response Fields:" << std::endl;
          for (const auto& pair : elementNameValueMap) {
            std::cout << "  " << pair.first << " = " << pair.second << std::endl;
          }
          
          // Check for CLIENT_ORDER_ID in response
          auto clientOrderIdIt = elementNameValueMap.find("CLIENT_ORDER_ID");
          if (clientOrderIdIt != elementNameValueMap.end()) {
            std::string clientOrderId = clientOrderIdIt->second;
            std::cout << "🔍 Found CLIENT_ORDER_ID in response: " << clientOrderId << std::endl;
            
            auto it = orderTimestamps.find(clientOrderId);
            if (it != orderTimestamps.end()) {
              // Use CCAPI's timeReceived - more accurate than our local clock
              auto ccapiReceiveTime = message.getTimeReceived(); // When CCAPI received the response
              auto serverProcessTime = message.getTime(); // Server timestamp when OKX processed the response
              
              // Check if server timestamp is valid (not Unix epoch)
              auto epochTime = std::chrono::time_point<std::chrono::system_clock>{};
              auto timeSinceEpoch = std::chrono::duration_cast<std::chrono::seconds>(serverProcessTime - epochTime);
              bool hasValidServerTime = timeSinceEpoch.count() > 1000000000; // More than ~31 years since epoch (roughly 2001+)
              
              // Calculate CCAPI latency (send → CCAPI received response)
              auto ccapiLatency = std::chrono::duration_cast<std::chrono::microseconds>(ccapiReceiveTime - it->second);
              
              std::cout << "⚡ LATENCY ANALYSIS (RESPONSE) - Client ID: " << clientOrderId << std::endl;
              std::cout << "  🌐 CCAPI Round-Trip Latency: " << ccapiLatency.count() 
                       << " μs (" << (ccapiLatency.count() / 1000.0) << " ms)" << std::endl;
              
              if (hasValidServerTime) {
                // 2. Server Processing Latency (only if server timestamp is valid)
                auto serverLatency = std::chrono::duration_cast<std::chrono::microseconds>(serverProcessTime - it->second);
                
                // 3. Network Delay (difference between the two)
                auto networkDelay = std::chrono::duration_cast<std::chrono::microseconds>(ccapiReceiveTime - serverProcessTime);
                
                // Store the server latency (more accurate) for statistics
                latencies.push_back(serverLatency.count());
                
                std::cout << "  📊 Server Processing Latency: " << serverLatency.count() 
                         << " μs (" << (serverLatency.count() / 1000.0) << " ms)" << std::endl;
                std::cout << "  📡 Network Delay: " << networkDelay.count() 
                         << " μs (" << (networkDelay.count() / 1000.0) << " ms)" << std::endl;
                std::cout << "  🎯 Server timestamp accuracy: VALID" << std::endl;
              } else {
                // Use CCAPI latency when server timestamp is invalid  
                latencies.push_back(ccapiLatency.count());
                std::cout << "  ⚠️  Server timestamp invalid (epoch time), using CCAPI latency" << std::endl;
                std::cout << "  📊 CCAPI Latency: " << ccapiLatency.count() 
                         << " μs (" << (ccapiLatency.count() / 1000.0) << " ms)" << std::endl;
              }
              
              orderTimestamps.erase(it);
            } else {
              std::cout << "❓ CLIENT_ORDER_ID not found in pending orders: " << clientOrderId << std::endl;
            }
          } else {
            std::cout << "❓ No CLIENT_ORDER_ID found in response" << std::endl;
          }
        }
      }
    }
  }

private:
  void placeTestOrder(Session* sessionPtr, double bestBid) {
    orderCount++;
    
    std::cout << "\n🚀 PLACING ORDER #" << orderCount << std::endl;
    
    // Generate unique client order ID (OKX format: alphanumeric, 1-32 chars)
    auto now = std::chrono::high_resolution_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    std::string clientOrderId = "ord" + std::to_string(timestamp % 100000000); // Use last 8 digits of timestamp
    std::string correlationId = "test_order_" + std::to_string(orderCount);
    
    // Calculate order price (bestBid * 0.8)
    double orderPrice = bestBid - 0.01;

    // Calculate quantity to buy for approximately 10 USDT (BTC-USDT)
    // quantity = 10 / orderPrice
    double usdtAmount = 1.0;
    double quantity = usdtAmount / orderPrice;

    // Format quantity to 6 decimals (BTC minimum step is usually 0.00001 or 0.0001)
    std::ostringstream quantityStream;
    quantityStream << std::fixed << std::setprecision(6) << quantity;
    std::string quantityStr = quantityStream.str();

    std::cout << "📊 Order Parameters:" << std::endl;
    std::cout << "  Client Order ID: " << clientOrderId << std::endl;
    std::cout << "  Correlation ID: " << correlationId << std::endl;
    std::cout << "  Best Bid: " << std::fixed << std::setprecision(2) << bestBid << std::endl;
    std::cout << "  Order Price: " << std::fixed << std::setprecision(2) << orderPrice << " (bestBid * 0.8)" << std::endl;
    std::cout << "  Quantity: " << quantityStr << " BTC (~10 USDT)" << std::endl;
    
    // Create order request
    Request request(Request::Operation::CREATE_ORDER, "okx", "BTC-USDT", correlationId);
    request.appendParam({
        {"SIDE", "BUY"},
        {"LIMIT_PRICE", std::to_string(orderPrice)},
        {"QUANTITY", quantityStr},
        {"CLIENT_ORDER_ID", clientOrderId},
    });
    
    std::cout << "📝 Request Parameters:" << std::endl;
    std::cout << "  SIDE: BUY" << std::endl;
    std::cout << "  LIMIT_PRICE: " << std::to_string(orderPrice) << std::endl;
    std::cout << "  QUANTITY: " << quantityStr << std::endl;
    std::cout << "  CLIENT_ORDER_ID: " << clientOrderId << std::endl;
    
    // Record timestamp before sending
    auto sendTime = std::chrono::high_resolution_clock::now();
    orderTimestamps[clientOrderId] = sendTime;
    // Print send timestamp in ISO format
    auto sendTimeT = std::chrono::system_clock::now();
    std::time_t sendTimeC = std::chrono::system_clock::to_time_t(sendTimeT);
    std::cout << "⏰ Order send timestamp (local, ISO): " << std::put_time(std::gmtime(&sendTimeC), "%Y-%m-%dT%H:%M:%SZ") << std::endl;
    
    std::cout << "⏰ Recording send timestamp for client order ID: " << clientOrderId << std::endl;
    std::cout << "📤 Sending order request..." << std::endl;
    
    // Send the order request
    sessionPtr->sendRequest(request);
    
    std::cout << "✅ Order request sent! Waiting for response..." << std::endl;
  }

public:
  std::atomic<int> getOrderCount() const {
    return orderCount.load();
  }

  std::atomic<int> getMessageCount() const {
    return messageCount.load();
  }

  double getAverageLatency() const {
    if (latencies.empty()) return 0.0;
    
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    return sum / latencies.size();
  }
  
  size_t getLatencyCount() const {
    return latencies.size();
  }
  
  std::vector<double> getLatencies() const {
    return latencies;
  }

  bool isMarketDataOnly() const {
    return marketDataOnly;
  }

private:
  std::atomic<int> orderCount;
  std::atomic<int> messageCount;
  std::chrono::high_resolution_clock::time_point lastOrderTime;
  std::map<std::string, std::chrono::high_resolution_clock::time_point> orderTimestamps;
  std::vector<double> latencies; // Store latencies in microseconds
  bool marketDataOnly;
};

} /* namespace ccapi */

using ::ccapi::MyEventHandler;
using ::ccapi::Session;
using ::ccapi::SessionConfigs;
using ::ccapi::SessionOptions;
using ::ccapi::Subscription;
using ::ccapi::UtilSystem;

bool checkCredentials() {
  return !UtilSystem::getEnvAsString("OKX_API_KEY").empty() &&
         !UtilSystem::getEnvAsString("OKX_API_SECRET").empty() &&
         !UtilSystem::getEnvAsString("OKX_API_PASSPHRASE").empty();
}

int main(int argc, char** argv) {
  // Parse command line arguments for sleep time (default 15 seconds)
  int sleepTime = 15;
  if (argc > 1) {
    try {
      sleepTime = std::stoi(argv[1]);
      if (sleepTime <= 0) {
        std::cerr << "Error: Sleep time must be positive. Using default of 15 seconds." << std::endl;
        sleepTime = 15;
      }
    } catch (const std::exception& e) {
      std::cerr << "Error parsing sleep time argument: " << e.what() << ". Using default of 15 seconds." << std::endl;
      sleepTime = 15;
    }
  }
  std::cout << "================================================================\n"
            << "| CCAPI OKX LATENCY TEST - ORDER PLACEMENT & RESPONSE TIMING  |\n" 
            << "================================================================\n" << std::endl;

  // Check if credentials are available
  bool hasCredentials = checkCredentials();
  bool marketDataOnly = !hasCredentials;

  if (!hasCredentials) {
    std::cout << "⚠️  No OKX API credentials found!" << std::endl;
    std::cout << "Running in MARKET DATA ONLY mode." << std::endl;
    std::cout << "To enable order latency testing, set these environment variables:" << std::endl;
    std::cout << "  export OKX_API_KEY=\"your_api_key_here\"" << std::endl;
    std::cout << "  export OKX_API_SECRET=\"your_api_secret_here\"" << std::endl;
    std::cout << "  export OKX_API_PASSPHRASE=\"your_passphrase_here\"" << std::endl;
    std::cout << std::endl;
  } else {
    std::cout << "✓ OKX API credentials found!" << std::endl;
    std::cout << "Running in FULL LATENCY TEST mode." << std::endl;
    std::cout << std::endl;
  }

  try {
    // Initialize ccapi session
    SessionOptions sessionOptions;
    SessionConfigs sessionConfigs;

    // <demo> - Configure demo endpoints
    std::cout << "🔧 CONFIGURING DEMO ENVIRONMENT..." << std::endl;
    
    // Set demo WebSocket endpoints
    std::map<std::string, std::string> urlWebsocketBase = sessionConfigs.getUrlWebsocketBase();
    urlWebsocketBase["okx"] = "wss://wspap.okx.com:8443";
    sessionConfigs.setUrlWebsocketBase(urlWebsocketBase);
    std::cout << "✓ Demo public WebSocket: wss://wspap.okx.com:8443" << std::endl;
    
    // Set demo REST endpoints  
    std::map<std::string, std::string> urlRestBase = sessionConfigs.getUrlRestBase();
    urlRestBase["okx"] = "https://www.okx.com";  // Demo environment uses same REST endpoint
    sessionConfigs.setUrlRestBase(urlRestBase);
    std::cout << "✓ Demo REST API: https://www.okx.com" << std::endl;
    

    
    // Verify credentials are loaded
    std::cout << "🔐 CREDENTIAL VERIFICATION:" << std::endl;
    std::cout << "✓ API Key: " << (UtilSystem::getEnvAsString("OKX_API_KEY").empty() ? "❌ MISSING" : "✓ Set (" + UtilSystem::getEnvAsString("OKX_API_KEY").substr(0, 8) + "...)") << std::endl;
    std::cout << "✓ API Secret: " << (UtilSystem::getEnvAsString("OKX_API_SECRET").empty() ? "❌ MISSING" : "✓ Set (***hidden***)") << std::endl;  
    std::cout << "✓ API Passphrase: " << (UtilSystem::getEnvAsString("OKX_API_PASSPHRASE").empty() ? "❌ MISSING" : "✓ Set (***hidden***)") << std::endl;
    std::cout << "✓ Simulated Trading: " << (UtilSystem::getEnvAsString("OKX_API_X_SIMULATED_TRADING").empty() ? "❌ MISSING (PRODUCTION MODE)" : "✓ Set (DEMO MODE)") << std::endl;
    std::cout << std::endl;
    // </demo>

    MyEventHandler eventHandler(marketDataOnly);
    Session session(sessionOptions, sessionConfigs, &eventHandler);

    std::cout << "================================================================\n"
              << "| SETTING UP SUBSCRIPTIONS                                     |\n"
              << "================================================================\n" << std::endl;

    // Subscribe to market data for BTC-USDT (always works)
    Subscription marketDataSubscription("okx", "BTC-USDT", "MARKET_DEPTH");
    session.subscribe(marketDataSubscription);
    std::cout << "✓ Subscribed to OKX BTC-USDT market depth data" << std::endl;

    // Only subscribe to order updates if we have credentials
    if (hasCredentials) {
      try {
        Subscription orderUpdateSubscription("okx", "BTC-USDT", "ORDER_UPDATE");
        session.subscribe(orderUpdateSubscription);
        std::cout << "✓ Attempting to subscribe to OKX order updates..." << std::endl;
        std::cout << "  (This may fail if credentials lack trading permissions)" << std::endl;
      } catch (const std::exception& e) {
        std::cerr << "⚠️  Failed to subscribe to order updates: " << e.what() << std::endl;
        std::cerr << "Continuing with market data only..." << std::endl;
        marketDataOnly = true;
      }
    }
    
    std::cout << "\n================================================================\n";
    if (marketDataOnly) {
      std::cout << "| STARTING MARKET DATA PERFORMANCE TEST                       |\n";
      std::cout << "================================================================\n" << std::endl;
      std::cout << "Mode: Market data subscription only" << std::endl;
      std::cout << "Monitoring: BTC-USDT best bid/ask updates" << std::endl;
    } else {
      std::cout << "| STARTING LATENCY TEST                                        |\n";
      std::cout << "================================================================\n" << std::endl;
      std::cout << "Algorithm: Place buy orders at (bestBid - 0.0001) every 10 seconds" << std::endl;
      std::cout << "Quantity: 0.01 BTC per order" << std::endl;
    }
    std::cout << "Duration: " << sleepTime << " seconds" << std::endl;
    std::cout << "Press Ctrl+C to stop early if needed.\n" << std::endl;

    // Sleep for the specified time with periodic status updates
    auto startTime = std::chrono::high_resolution_clock::now();
    const int statusInterval = 5; // Print status every 5 seconds
    
    for (int elapsed = 0; elapsed < sleepTime; elapsed += statusInterval) {
      std::this_thread::sleep_for(std::chrono::seconds(statusInterval));
      
      int currentMessages = eventHandler.getMessageCount();
      int currentOrders = eventHandler.getOrderCount();
      
      std::cout << "\n--- STATUS UPDATE (t+" << (elapsed + statusInterval) << "s) ---" << std::endl;
      std::cout << "Market data messages received: " << currentMessages << std::endl;
      std::cout << "Orders placed: " << currentOrders << std::endl;
      
      if (currentMessages == 0 && elapsed >= 10) {
        std::cout << "⚠️  No market data received yet. This might indicate:" << std::endl;
        std::cout << "  1. Demo endpoint has no live data" << std::endl;
        std::cout << "  2. Different market data field names are used" << std::endl;
        std::cout << "  3. Subscription channel mismatch" << std::endl;
      }
    }
    
    // Stop the session
    session.stop();
    
    // Print final statistics
    int finalOrderCount = eventHandler.getOrderCount();
    int finalMessageCount = eventHandler.getMessageCount();
    double averageLatency = eventHandler.getAverageLatency();
    size_t responseCount = eventHandler.getLatencyCount();
    auto latencies = eventHandler.getLatencies();
    
    std::cout << "\n================================================================\n";
    if (eventHandler.isMarketDataOnly()) {
      std::cout << "| MARKET DATA PERFORMANCE RESULTS                             |\n";
      std::cout << "================================================================\n" << std::endl;
      std::cout << "Total market data messages received: " << finalMessageCount << std::endl;
      std::cout << "Average messages per second: " 
                << (finalMessageCount / static_cast<double>(sleepTime)) << std::endl;
    } else {
      std::cout << "| LATENCY TEST RESULTS                                         |\n";
      std::cout << "================================================================\n" << std::endl;
      
      std::cout << "Total market data messages received: " << finalMessageCount << std::endl;
      std::cout << "Total orders placed: " << finalOrderCount << std::endl;
      std::cout << "Total responses received: " << responseCount << std::endl;
      std::cout << "Response rate: " << (finalOrderCount > 0 ? (responseCount * 100.0 / finalOrderCount) : 0) << "%" << std::endl;
      
      if (responseCount > 0) {
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "📊 Server Processing Latency Statistics (More Accurate):" << std::endl;
        std::cout << "Average server latency: " << averageLatency << " microseconds (" 
                  << (averageLatency / 1000.0) << " milliseconds)" << std::endl;
        
        // Calculate min/max latency
        auto minmax = std::minmax_element(latencies.begin(), latencies.end());
        std::cout << "Minimum server latency: " << *minmax.first << " microseconds (" 
                  << (*minmax.first / 1000.0) << " ms)" << std::endl;
        std::cout << "Maximum server latency: " << *minmax.second << " microseconds (" 
                  << (*minmax.second / 1000.0) << " ms)" << std::endl;
        
        // Calculate percentiles
        std::vector<double> sortedLatencies = latencies;
        std::sort(sortedLatencies.begin(), sortedLatencies.end());
        
        if (sortedLatencies.size() >= 2) {
          size_t p50_idx = sortedLatencies.size() * 0.5;
          size_t p95_idx = sortedLatencies.size() * 0.95;
          size_t p99_idx = sortedLatencies.size() * 0.99;
          
          std::cout << "50th percentile (P50): " << sortedLatencies[p50_idx] << " microseconds (" 
                    << (sortedLatencies[p50_idx] / 1000.0) << " ms)" << std::endl;
          std::cout << "95th percentile (P95): " << sortedLatencies[p95_idx] << " microseconds (" 
                    << (sortedLatencies[p95_idx] / 1000.0) << " ms)" << std::endl;
          std::cout << "99th percentile (P99): " << sortedLatencies[p99_idx] << " microseconds (" 
                    << (sortedLatencies[p99_idx] / 1000.0) << " ms)" << std::endl;
        }
      } else {
        std::cout << "No order responses received - check API credentials and connectivity" << std::endl;
      }
    }
    
    std::cout << "Runtime: " << sleepTime << " seconds" << std::endl;
    std::cout << "================================================================\n" << std::endl;
    
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "CCAPI test completed successfully. Bye!" << std::endl;
  return EXIT_SUCCESS;
} 