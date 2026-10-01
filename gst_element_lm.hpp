#pragma once

#include <gst/gstbin.h>
#include <gst/gstelement.h>
#include <gst/gstutils.h>
#include <iostream>
#include <stdexcept>

// Helper Life Management class for GstElement
// It takes care of unrefing the GstElement when it goes out of scope
// Also it makes sure if the element passed is null
class GstElementLM {
    
    public:
        // Default constructor
        // Necessary for automatic class initialization in FRPipeline
        GstElementLM() : element(nullptr) {}

        GstElementLM(GstElement* element) : element(nullptr) {
            if (!element) {
                throw std::runtime_error("Failed to create GstElement");
            }
            if (this->element) {
                throw std::runtime_error("GstElement is already set");
            }
            this->element = element;
        }

        GstElementLM(const GstElementLM& other) = delete;        
        GstElementLM& operator=(const GstElementLM& other) = delete;

        GstElementLM(GstElementLM&& other) noexcept{
            this->element = other.element;
            other.element = nullptr;
        }

        GstElementLM& operator=(GstElementLM&& other) noexcept {
            this->element = other.element;
            other.element = nullptr;

            return *this;
        }

        ~GstElementLM() {
            if (element) {
                std::cout << "Unrefing GstElement: " << GST_ELEMENT_NAME(element) << std::endl;
                gst_object_unref(element);
            }
            else {
                std::cout << "Skipping unrefing GstElement since it is null" << std::endl;
            }
        }

        GstElement* unpack() {
            if (!element) {
                throw std::runtime_error("GstElement is not set");
            }

            GstElement* temp = element;
            element = nullptr;
            return temp;
        }
        

        GstElement* get() const {
            return element;
        }

    protected:
        GstElement* element;
};

class GstPipelineLM: public GstElementLM {
    public:
        GstPipelineLM() : GstElementLM() {}
        GstPipelineLM(GstElement* element) : GstElementLM(element) {}

        GstPipelineLM(GstPipelineLM&& other) noexcept{
            this->element = other.element;
            other.element = nullptr;
        }

        GstPipelineLM& operator=(GstPipelineLM&& other) noexcept {
            this->element = other.element;
            other.element = nullptr;

            return *this;
        }

        ~GstPipelineLM() {
            if (get()) {
                gst_element_set_state(get(), GST_STATE_NULL);
            }
        }

        GstElement* unpack() = delete;

        GstElement* get_by_name(const std::string& name) const {
            return gst_bin_get_by_name(GST_BIN(get()), name.c_str());
        }

        void add_to_pipeline(GstElementLM&& element) {
            bool res = gst_bin_add(GST_BIN(get()), element.get());

            if (res != TRUE) {
                g_printerr("Element could not be added to pipeline.\n");
                throw std::runtime_error("Failed to add element to pipeline");
            }
            
            element.unpack();
        }

        void link_elements(const std::string& el1, const std::string& el2) {
            GstElement* element1 = gst_bin_get_by_name(GST_BIN(get()), el1.c_str());
            GstElement* element2 = gst_bin_get_by_name(GST_BIN(get()), el2.c_str());

            if (gst_element_link(element1, element2) != TRUE) {
                g_printerr("Elements could not be linked.\n");
                throw std::runtime_error("Pipeline elements could not be linked");
            }
        }
};

class GstBusLM {
    public:
        GstBusLM() : bus(nullptr) {}
        GstBusLM(GstBus* bus) : bus(nullptr) {
            if (!bus) {
                throw std::runtime_error("Failed to create GstBus");
            }
            if (this->bus) {
                throw std::runtime_error("GstBus is already set");
            }
            this->bus = bus;
        }

        GstBusLM(const GstBusLM& other) = delete;        
        GstBusLM& operator=(const GstBusLM& other) = delete;

        GstBusLM(GstBusLM&& other) noexcept{
            this->bus = other.bus;
            other.bus = nullptr;
        }

        GstBusLM& operator=(GstBusLM&& other) noexcept {
            this->bus = other.bus;
            other.bus = nullptr;

            return *this;
        }

        ~GstBusLM() {
            if (bus) {
                std::cout << "Unrefing GstBus" << std::endl;
                gst_object_unref(bus);
            }
            else {
                std::cout << "Skipping unrefing GstBus since it is null" << std::endl;
            }
        }

        GstBus* get() const {
            return bus;
        }

    protected:
        GstBus* bus;
};

class GstMessageLM {
    public:
        GstMessageLM() : msg(nullptr) {}
        GstMessageLM(GstMessage* msg) : msg(nullptr) {
            if (!msg) {
                throw std::runtime_error("Failed to create GstMessage");
            }
            if (this->msg) {
                throw std::runtime_error("GstMessage is already set");
            }
            this->msg = msg;
        }

        GstMessageLM(const GstMessageLM& other) = delete;        
        GstMessageLM& operator=(const GstMessageLM& other) = delete;

        GstMessageLM(GstMessageLM&& other) noexcept{
            this->msg = other.msg;
            other.msg = nullptr;
        }

        GstMessageLM& operator=(GstMessageLM&& other) noexcept {
            this->msg = other.msg;
            other.msg = nullptr;

            return *this;
        }

        ~GstMessageLM() {
            if (msg) {
                std::cout << "Unrefing GstMessage: " << GST_MESSAGE_TYPE(msg) << std::endl;
                gst_message_unref(msg);
            }
            else {
                std::cout << "Skipping unrefing GstMessage since it is null" << std::endl;
            }
        }

        GstMessage* get() const {
            return msg;
        }

    protected:
        GstMessage* msg;
};

