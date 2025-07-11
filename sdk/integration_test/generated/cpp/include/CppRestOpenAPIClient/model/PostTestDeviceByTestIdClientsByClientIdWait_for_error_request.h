/**
 * Elysia Documentation
 * Development documentation
 *
 * The version of the OpenAPI document: 0.0.0
 *
 * NOTE: This class is auto generated by OpenAPI-Generator 7.14.0.
 * https://openapi-generator.tech
 * Do not edit the class manually.
 */

/*
 * PostTestDeviceByTestIdClientsByClientIdWait_for_error_request.h
 *
 * 
 */

#ifndef ORG_OPENAPITOOLS_CLIENT_MODEL_PostTestDeviceByTestIdClientsByClientIdWait_for_error_request_H_
#define ORG_OPENAPITOOLS_CLIENT_MODEL_PostTestDeviceByTestIdClientsByClientIdWait_for_error_request_H_


#include "CppRestOpenAPIClient/ModelBase.h"


namespace org {
namespace openapitools {
namespace client {
namespace model {



class  PostTestDeviceByTestIdClientsByClientIdWait_for_error_request
    : public ModelBase
{
public:
    PostTestDeviceByTestIdClientsByClientIdWait_for_error_request();
    virtual ~PostTestDeviceByTestIdClientsByClientIdWait_for_error_request();

    /////////////////////////////////////////////
    /// ModelBase overrides

    void validate() override;

    web::json::value toJson() const override;
    bool fromJson(const web::json::value& json) override;

    void toMultipart(std::shared_ptr<MultipartFormData> multipart, const utility::string_t& namePrefix) const override;
    bool fromMultiPart(std::shared_ptr<MultipartFormData> multipart, const utility::string_t& namePrefix) override;


    /////////////////////////////////////////////
    /// PostTestDeviceByTestIdClientsByClientIdWait_for_error_request members


    double getTimeout() const;
    bool timeoutIsSet() const;
    void unsetTimeout();
    void setTimeout(double value);


protected:
    double m_Timeout;
    bool m_TimeoutIsSet;

};


}
}
}
}

#endif /* ORG_OPENAPITOOLS_CLIENT_MODEL_PostTestDeviceByTestIdClientsByClientIdWait_for_error_request_H_ */
